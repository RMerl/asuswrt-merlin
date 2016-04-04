var dragenterHandler = function (event) {
	openUploadPanel(1);
	handleDragOver(event);
};
		
function create_ui_view(type, container, query_type, parent_url, folder_array, file_array, mousedown_item_handler){
	
	var cancel_all_select_items = function(){
		g_select_array = null;
		g_select_file_count=0;
		g_select_folder_count=0;
		
		$("#fileview .wcb").each(function(){				
			$(this).find(".item-check").removeClass("x-view-selected");
			$(this).find(".item-check").hide();
			$(this).find(".item-menu").removeClass("x-view-menu-popup");
			$(this).find(".item-menu").hide();
			$(this).css("background-color", "");
		});
		
		refreshSelectWindow();
	};
	
	var select_item = function(select_item){
		var item_check = select_item.find(".item-check");
		
		if(item_check.hasClass("x-view-selected")){
			//- unselect
			item_check.removeClass("x-view-selected");
			select_item.css("background-color", "");
			item_check.show();
		}
		else{
			//- select
			item_check.addClass("x-view-selected");
			select_item.css("background-color", "#00C2EB");
			item_check.show();			
		}
		
		g_select_array = null;
		g_select_array = new Array();		
		g_select_file_count=0;
		g_select_folder_count=0;
		
		$("#fileview").find(".wcb").each(function(){
			if($(this).find(".item-check").hasClass("x-view-selected")){
				var uhref = $(this).attr("uhref");
				var isdir = $(this).attr("isdir");
				var title = myencodeURI($(this).attr("data-name"));
				
				if(isdir==1){					
					g_select_folder_count++;
				}
				else{
					g_select_file_count++;
				}
				
				g_select_array.push( { isdir:isdir, uhref:uhref, title:title } );
    		}
		});	
		
		refreshSelectWindow();
	};
	
	container.empty();
	
	if(query_type==2)
		return;
	
	if(type=="thumbview"){
		createThumbView(container, query_type, parent_url, folder_array, file_array, mousedown_item_handler);
	}
	else if(type=="listview"){
		createListView(container, query_type, parent_url, folder_array, file_array, mousedown_item_handler)
	}
	
	if(g_support_html5==1){		
		var dropZone = document.getElementById('fileview');
		if(query_type==0){
			dropZone.addEventListener('dragenter', dragenterHandler, false);
		}
		else{
			dropZone.removeEventListener('dragenter', dragenterHandler, false);
		}
  	}
	
	$(".item-check").click(function(){
		var wcb = $(this).parents(".wcb");
		select_item(wcb);
	});
	
	$('.item-menu').click( function(e){
		var item_menu = $(this);
		var wcb = $(this).parents(".wcb");
		
		//- Cancel all select item.
		cancel_all_select_items();
		
        if(item_menu.hasClass("x-view-menu-popup")){
			item_menu.removeClass("x-view-menu-popup");
			wcb.css("background-color", "");
		}
		else{
			item_menu.addClass("x-view-menu-popup");
			wcb.css("background-color", "#00C2EB");
			item_menu.show();
		}
		
		//refreshSelectWindow();
		select_item(wcb);
    });
	
	$("#fileview .fcb").mouseover(function(){
		$(this).find(".item-check").show();
		$(this).find(".item-menu").show();
		
		$(this).css("background-color", "#00C2EB");
	});
	
	$("#fileview .fcb").mouseleave(function(){
		var thumb_check = $(this).find(".item-check");
		var thumb_menu = $(this).find(".item-menu");
		
		if( !thumb_check.hasClass("x-view-selected") ){
			$(this).find(".item-check").hide();
		}
		
		if( !thumb_menu.hasClass("x-view-menu-popup") ){
			$(this).find(".item-menu").hide();
			
		}
		
		if( !thumb_check.hasClass("x-view-selected") && 
		    !thumb_menu.hasClass("x-view-menu-popup") ){
			$(this).css("background-color", "");
		}	
	});
}

function createThumbView(container, query_type, parent_url, folder_array, file_array, mousedown_item_handler){
	var html = "";
	
	//- Parent Path
	if(query_type == 0&&parent_url!="") {
		html += '<div class="albumDiv fcb" qtype="1" isParent="1" isdir="1" uhref="' + parent_url + '">';
		html += '<table class="thumb-table-parent">';
		html += '<tbody>';
		html += '<tr><td>';
		html += '<div class="picDiv cb">';
		html += '<div class="parentDiv bicon"></div></div>';
		html += '</td></tr>';
		html += '<tr><td>';
		html += '<div class="albuminfo">';
		html += '<a id="list_item" title="' + m.getString("btn_prevpage") + '">' + m.getString("btn_prevpage") + '</a>';
		html += '</div>';
		html += '</td></tr>';
		html += '</tbody>';
		html += '</table>';
		html += '</div>';
	}
		
	var isInUSBFolder = 0;
	
	//- Folder List
	for(var i=0; i<folder_array.length; i++){
		var this_title = m.getString("table_filename") + ": " + folder_array[i].name; 
		
		if(folder_array[i].time!="")
			this_title += "\n" + m.getString("table_time") + ": " + folder_array[i].time;
		                 							
		html += '<div class="albumDiv fcb wcb" ';
		html += ' title="';
		html += this_title;
		html += '" qtype="';
		html += query_type;
		html += '" isParent="0" isdir="1" uhref="';
		html += folder_array[i].href;
		html += '" data-name="';
		html += folder_array[i].name;
		html += '" data-thumb="';
		html += folder_array[i].thumb;
		html += '" online="';
		html += folder_array[i].online;
		html += '" ip="" mac="';
		html += folder_array[i].mac;
		html += '" uid="';
		html += folder_array[i].uid;
		html += '" freadonly="';
		html += folder_array[i].freadonly;
		html += '" fhidden="';
		html += folder_array[i].fhidden;
		if(folder_array[i].type == "usbdisk") html += '" isusb="1"';
		else html += '" isusb="0"';
		html += '">';
		
		html += '<table class="thumb-table-parent">';
		html += '<tbody>';
					
		html += '<tr><td>';
		html += '<div class="picDiv cb" popupmenu="';
									
		if(query_type == "0")
			html += '1';
		else
			html += '0';
									
		html += '" uhref="';
		html += folder_array[i].href;
		//html += '" data-thumb="';
		//html += folder_array[i].thumb;
		//html += '" data-name="';
		//html += folder_array[i].name;
		html += '">';
		
		if(query_type == "2"){
			//- Host Query
			if( folder_array[i].type == "usbdisk" )
				html += '<div id="fileviewicon" class="usbDiv bicon">';
			else{
				if(folder_array[i].online == "1")
					html += '<div id="fileviewicon" class="computerDiv bicon">';
				else
					html += '<div id="fileviewicon" class="computerOffDiv bicon">';
			}
		}
		else {
			/*
			if(folder_array[i].thumb=="1"){
				html += '<div id="fileviewicon">';
				html += '<img src="data:image/jpeg;base64,' + folder_array[i].thumb + '" width="80px" height="60px" onload="javascript:DrawImage(this,80,60);"></img>';
			}
			else{
				html += '<div id="fileviewicon" class="folderDiv bicon">';
			}
			*/
			html += '<div id="fileviewicon" class="folderDiv bicon">';
		}
		
		//- Show router sync icon.
		if( folder_array[i].routersyncfolder == "1" ){
			html += '<div id="routersyncicon" class="routersyncDiv sicon"></div>';
		}
			
		html += '</div></div>';								
		html += '</td></tr>';
		html += '<tr><td>';
		html += '<div class="albuminfo">';
		html += '<a id="list_item">';
		
		html += folder_array[i].shortname;
								
		if(folder_array[i].online == "0" && query_type == "2")
			html += "(" + m.getString("title_offline") + ")";
									
		html += '</a>';
		html += '</div>';
		html += '</td></tr>';
		html += '</tbody>';
		html += '</table>';
		
		if(query_type == "0"){
			html += '<div class="item-check ui-icon"></div>';
			html += '<div class="item-menu ui-icon"></div>';
		}
		
		html += '</div>';
		/*
		if(folder_array[i].type == "usbdisk"&&query_type==1&&g_fileview_only==0){
			client.GETDISKSPACE("/", folder_array[i].name, function(error, statusstring, content){				
				if(error==200){
					var data = parseXml(content);
					var x = $(data);	
					
					var queryTag = $('a#list_item[title="' + x.find("DiskName").text() + '"]').parents(".albumDiv");
					
					var title = queryTag.attr("title") + "\n" + m.getString("table_diskusedpercent") + ": " + x.find("DiskUsedPercent").text();
					queryTag.attr("title", title);
				}
			});	
			
			isInUSBFolder = 1;				
		}
		*/
	}
	
	//- File List
	for(var i=0; i<file_array.length; i++){
		var this_title = m.getString("table_filename") + ": " + file_array[i].name + "\n" + 
		                 m.getString("table_time") + ": " + file_array[i].time + "\n" + 
		                 m.getString("table_size") + ": " + file_array[i].size;
		
		html += '<div class="albumDiv fcb wcb"';
		html += ' title="';
		html += this_title;		
		html += '" qtype="1" isParent="1" isdir="0" uhref="';
		html += file_array[i].href;
		html += '" data-name="';
		html += file_array[i].name;
		html += '" matadatatitle="';
		html += file_array[i].matadatatitle;
		html += '" uid="';
		html += file_array[i].uid;
		html += '" ext="';
		html += file_ext;
		html += '" freadonly="';
		html += file_array[i].freadonly;
		html += '" fhidden="';
		html += file_array[i].fhidden;
		html += '" data-thumb="';
		html += file_array[i].thumb;
		if(file_array[i].type == "usbdisk") html += '" isusb="1"';
		else html += '" isusb="0"';
		html += '">';
		
		html += '<table class="thumb-table-parent">';
		html += '<tbody>';									
									
		//- get file ext
		var file_path = String(file_array[i].href);
		var file_ext = getFileExt(file_path);
		if(file_ext.length>5)file_ext="";
																
		html += '<tr><td>';
		if(query_type == "0")
			html += '<div class="picDiv cb" popupmenu="1" uhref="';
		else
			html += '<div class="picDiv cb" popupmenu="0" uhref="';
		html += file_array[i].href;
		//html += '" data-thumb="';
		//html += file_array[i].thumb;
		//html += '" data-name="';
		//html += file_array[i].name;
		html += '">';
		
		/*
		if(file_array[i].thumb=="1"){
			html += '<div id="fileviewicon">';
			html += '<img src="data:image/jpeg;base64,' + file_array[i].thumb + '" width="80px" height="60px" onload="javascript:DrawImage(this,80,60);"></img>';
		}
		else{
		*/
		
		if(file_ext=="jpg"||file_ext=="jpeg"||file_ext=="png"||file_ext=="gif"||file_ext=="bmp")
			html += '<div id="fileviewicon" class="imgfileDiv bicon">';
		else if(file_ext=="mp3"||file_ext=="m4a"||file_ext=="m4r"||file_ext=="wav")
			html += '<div id="fileviewicon" class="audiofileDiv bicon">';
		else if(file_ext=="mp4"||file_ext=="rmvb"||file_ext=="m4v"||file_ext=="wmv"||file_ext=="avi"||file_ext=="mpg"||
			      file_ext=="mpeg"||file_ext=="mkv"||file_ext=="mov"||file_ext=="flv"||file_ext=="3gp"||file_ext=="m2v"||file_ext=="rm")
			html += '<div id="fileviewicon" class="videofileDiv bicon">';
		else if(file_ext=="doc"||file_ext=="docx")
			html += '<div id="fileviewicon" class="docfileDiv bicon">';
		else if(file_ext=="ppt"||file_ext=="pptx")
			html += '<div id="fileviewicon" class="pptfileDiv bicon">';

		else if(file_ext=="xls"||file_ext=="xlsx")
			html += '<div id="fileviewicon" class="xlsfileDiv bicon">';
		else if(file_ext=="pdf")
			html += '<div id="fileviewicon" class="pdffileDiv bicon">';
		else{
			html += '<div id="fileviewicon" class="fileDiv bicon">';
		}
		//}
			
		html += '</div></div>';
		
		html += '</td></tr>';
		html += '<tr><td>';
		html += '<div class="albuminfo" style="font-size:80%">';
		html += '<a id="list_item">';
		/*
		html += '<a id="list_item" qtype="1" isdir="0" uhref="';
		html += file_array[i].href;
		html += '" title="';
		html += file_array[i].name;
		html += '" matadatatitle="';
		html += file_array[i].matadatatitle;
		html += '" uid="';
		html += file_array[i].uid;
		html += '" ext="';
		html += file_ext;
		html += '" freadonly="';
		html += file_array[i].freadonly;
		html += '" fhidden="';
		html += file_array[i].fhidden;
		html += '">';
		*/
		html += file_array[i].shortname;
		html += '</a>';
		html += '</div>';
		html += '</td></tr>';
		html += '</tbody>';
		html += '</table>';
		
		if(query_type == "0"){
			html += '<div class="item-check ui-icon"></div>';
			html += '<div class="item-menu ui-icon"></div>';
		}
		
		html += '</div>';
	}
		
	container.append(html);
	
	$(".thumb-table-parent").mousedown( function(e){
		var list_item = $(this).parents(".fcb");
		mousedown_item_handler(e, list_item);
	});
}

function createListView(container, query_type, parent_url, folder_array, file_array, mousedown_item_handler){
	var html = "";
	
	html += '<table id="ntl" class="table-file-list">';
	html += '<thead>';
	html += '<tr>';
	//html += '<th style="width:3%"></th>';
	//html += '<th style="width:4%"></th>';
	html += '<th style="width:30px"></th>';
	html += '<th style="width:40px"></th>';
	html += '<th style="width:58%">' + m.getString('table_filename') + '</th>';
	html += '<th style="width:25%">' + m.getString('table_time') + '</th>';
	html += '<th style="width:7%">' + m.getString('table_size') + '</th>';
	//html += '<th style="width:3%"></th>';
	html += '<th style="width:30px"></th>';
	html += '</tr>';
	html += '</thead>';
	
	html += '<tbody>';
	
	//- Parent Path
	if(query_type == 0&&parent_url!="") {
		
		html += '<tr class="listDiv fcb cbp" ';
		html += 'qtype="1" isParent="1" isdir="1" uhref="';
		html += parent_url;
		html += '" title="' + m.getString("btn_prevpage") + '" online="0">';
		html += '<td field="check"></td>';
		html += '<td field="icon"><div id="fileviewicon" class="parentDiv sicon"></td>';
		html += '<td field="filename" align="left">' + m.getString("btn_prevpage") + '</td>';
		html += '<td field="time" align="left"></td>';
		html += '<td field="size" align="left"></td>';
		html += '<td field="option" align="left"></td>';
		html += '</tr>';
	}
			
	var isInUSBFolder = 0;
	
	//- Folder List
	for(var i=0; i<folder_array.length; i++){
		
		html += '<tr class="listDiv fcb wcb"';
		html += ' id="list_item" qtype="';
		html += query_type;
		html += '" isdir="1" uhref="';
		html += folder_array[i].href;
		html += '" title="';
		html += folder_array[i].name;
		html += '" online="';
		html += folder_array[i].online;
		html += '" ip="" mac="';
		html += folder_array[i].mac;
		html += '" uid="';
		html += folder_array[i].uid;
		html += '" freadonly="';
		html += folder_array[i].freadonly;
		html += '" fhidden="';
		html += folder_array[i].fhidden;
		html += '" data-thumb="';
		html += folder_array[i].thumb;
		html += '" data-name="';
		html += folder_array[i].name;
		if(folder_array[i].type == "usbdisk") html += '" isusb="1"';
		else html += '" isusb="0"';
		html += '">';
		              							
		//html += '<td field="check"><input type="checkbox" id="check_del" name="check_del" class="check_del"></td>';
		html += '<td field="check">';
		if(query_type == "0"){
			html += '<div class="checklist">';
			html += '<div class="item-check ui-icon"></div>';	
			html += '</div>';
		}
		html += '</td>';
		
		html += '<td field="icon">';		
		if(query_type == "2"){
			//- Host Query
			if( folder_array[i].type == "usbdisk" )
				html += '<div id="fileviewicon" class="usbDiv sicon">';
			else{
				if(folder_array[i].online == "1")
					html += '<div id="fileviewicon" class="computerDiv sicon">';
				else
					html += '<div id="fileviewicon" class="computerOffDiv sicon">';
			}
		}
		else {
			html += '<div id="fileviewicon" class="folderDiv sicon">';
		}
		html += '</td>';
		
		html += '<td field="filename" align="left">' + folder_array[i].name + '</td>';
		html += '<td field="time" align="left">' + folder_array[i].time + '</td>';
		html += '<td field="size" align="left"></td>';
		html += '<td field="option" align="left">';
		if(query_type == "0"){
			html += '<div class="checklist">';
			html += '<div class="item-menu ui-icon"></div>';
			html += '</div>';
		}
		html += '</td>';
		html += '</tr>';		
	}
	
	//- File List
	for(var i=0; i<file_array.length; i++){
		
		//- get file ext
		var file_path = String(file_array[i].href);
		var file_ext = getFileExt(file_path);
		if(file_ext.length>5)file_ext="";
		
		html += '<tr class="listDiv fcb wcb"';		
		html += ' id="list_item" qtype="1" isdir="0" uhref="';
		html += file_array[i].href;
		html += '" title="';
		html += file_array[i].name;
		html += '" matadatatitle="';
		html += file_array[i].matadatatitle;
		html += '" uid="';
		html += file_array[i].uid;
		html += '" ext="';
		html += file_ext;
		html += '" freadonly="';
		html += file_array[i].freadonly;
		html += '" fhidden="';
		html += file_array[i].fhidden;
		html += '" data-thumb="';
		html += file_array[i].thumb;
		html += '" data-name="';
		html += file_array[i].name;
		if(file_array[i].type == "usbdisk") html += '" isusb="1"';
		else html += '" isusb="0"';
		html += '">';
		
		//html += '<td field="check"><input type="checkbox" id="check_del" name="check_del" class="check_del"></td>';
		
		html += '<td field="check">';
		if(query_type == "0"){
			html += '<div class="checklist">';
			html += '<div class="item-check ui-icon"></div>';
			html += '</div>';
		}
		html += '</td>';
		
		html += '<td field="icon">';
		
		if(file_ext=="jpg"||file_ext=="jpeg"||file_ext=="png"||file_ext=="gif"||file_ext=="bmp")
			html += '<div id="fileviewicon" class="imgfileDiv sicon">';
		else if(file_ext=="mp3"||file_ext=="m4a"||file_ext=="m4r"||file_ext=="wav")
			html += '<div id="fileviewicon" class="audiofileDiv sicon">';
		else if(file_ext=="mp4"||file_ext=="rmvb"||file_ext=="m4v"||file_ext=="wmv"||file_ext=="avi"||file_ext=="mpg"||
				  file_ext=="mpeg"||file_ext=="mkv"||file_ext=="mov"||file_ext=="flv"||file_ext=="3gp"||file_ext=="m2v"||file_ext=="rm")
			html += '<div id="fileviewicon" class="videofileDiv sicon">';
		else if(file_ext=="doc"||file_ext=="docx")
			html += '<div id="fileviewicon" class="docfileDiv sicon">';
		else if(file_ext=="ppt"||file_ext=="pptx")
			html += '<div id="fileviewicon" class="pptfileDiv sicon">';
		else if(file_ext=="xls"||file_ext=="xlsx")
			html += '<div id="fileviewicon" class="xlsfileDiv sicon">';
		else if(file_ext=="pdf")
			html += '<div id="fileviewicon" class="pdffileDiv sicon">';
		else{
			html += '<div id="fileviewicon" class="fileDiv sicon">';
		}
			
		html += '</td>';
		
		html += '<td field="filename" align="left">' + file_array[i].name + '</td>';		
		html += '<td field="time" align="left">' + file_array[i].time + '</td>';
		html += '<td field="size" align="left">' + file_array[i].size + '</td>';
		html += '<td field="option" align="left">';
		if(query_type == "0"){
			html += '<div class="checklist">';
			html += '<div class="item-menu ui-icon"></div>';
			html += '</div>';
		}
		html += '</td>';
		html += '</tr>';
	}
	
	html += '</tbody>';
	html += '</table>';
	
	html += '<table id="header-fixed" class="table-file-list"></table>';
		
	container.append(html);
	
	$("td[field=icon], td[field=filename], td[field=time], td[field=size]").mousedown( function(e){
		mousedown_item_handler(e, $(this).parent(".listDiv"));
	});
	
	var tableOffset = $("#ntl").offset().top;
	var $header = $("#ntl > thead").clone();
	var $fixedHeader = $("#header-fixed").append($header);
	$("#header-fixed").css("width", $("#ntl").width());
	
	$("#fileview").bind("scroll", function() {
		var offset = $(this).scrollTop();
		if (offset >= tableOffset && $fixedHeader.is(":hidden")) {
			$fixedHeader.show();
		}
		else if (offset < tableOffset) {
			$fixedHeader.hide();
		}
	});
}