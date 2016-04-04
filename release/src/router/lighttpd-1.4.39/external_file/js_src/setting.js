var m = new lang();
var g_storage = new myStorage();
var server_url = "http://oauth.asus.com/aicloud/";
var g_bLoginFacebook = false;
var g_bLoginGoogle = false;
var g_key_file = null;
var g_crt_file = null;
var g_intermediate_crt_file = null;
var g_create_account_dialog = null;
var g_account_info_dialog = null;
var g_invite_token = "";

$("document").ready(function() {
	//document.oncontextmenu = function() {return false;};
	
	var vars = getUrlVars();
	var loc_lan = String(window.navigator.userLanguage || window.navigator.language).toLowerCase();		
	var lan = ( g_storage.get('lan') == undefined ) ? loc_lan : g_storage.get('lan');
	m.setLanguage(lan);
	
	var init_page = ( vars["p"] == undefined ) ? 0 : vars["p"];
	var only_show_init_page = ( vars["s"] == undefined ) ? 0 : vars["s"];
		
	// 預設顯示第一個 Tab
	var _showTabIndex = init_page;
	$('ul.tabs li').eq(_showTabIndex).addClass('active');
	$('.tab_content').hide().eq(_showTabIndex).show();
	
	if(only_show_init_page){
		$('ul.tabs li').each(function(i){
			if(i!=_showTabIndex)
				$('ul.tabs li').eq(i).hide();
		});
	}
	
	// 當 li 頁籤被點擊時...
	// 若要改成滑鼠移到 li 頁籤就切換時, 把 click 改成 mouseover
	$('ul.tabs li').click(function() {
		// 找出 li 中的超連結 href(#id)
		var $this = $(this),
			_clickTab = $this.find('a').attr('href');
		// 把目前點擊到的 li 頁籤加上 .active
		// 並把兄弟元素中有 .active 的都移除 class
		$this.addClass('active').siblings('.active').removeClass('active');
		// 淡入相對應的內容並隱藏兄弟元素
		$(_clickTab).stop(false, true).fadeIn().siblings().hide();
 		 		
 		//if(_clickTab=="#tab3")
 		//	getLatestVersion();

		return false;
	}).find('a').focus(function(){
		this.blur();
	});
	
	$("button#btn_rescan").click(function(){
		$("button#btn_rescan").attr("disabled", true);
		$("button#btn_rescan").text("Wait");
		parent.doRescanSamba();
	});
	
	$("button#btn_gen_crt").click(function(){
		var client = new davlib.DavClient();
		client.initialize();
		
		var keylen = 2048;
		var caname = "AiCloud";
		var email = "";
		var country = "TW";
		var state = "Taiwan";
		var ln = "Hsinchu";
		var orag = "ASUS";
		var ounit = "RD";
		var cn = window.location.hostname;
		
		var msg = m.getString("msg_gen_crt_confirm");
		msg = msg.replace("%s", cn);
		
		if(confirm(msg)){
			$("#crt_creating").show();
			
			client.GENROOTCERTIFICATE("/", keylen,caname,email,country,state,ln,orag,ounit,cn, function(error, content, statusstring){	
				if(error==200){
					$("#crt_creating").hide();
					alert(m.getString("msg_gen_crt_complete"));
					
					client.APPLYAPP("/", "apply", "", "restart_webdav", function(error, statusstring, content){				
						if(error==200){
							setTimeout( function(){
								parent.location.reload(false);
							}, 6000 );
						}
						
						client = null;
					});					
				}
			});		
		}		
	});
	
	$("button#btn_import_crt").click(function(){
		
		$("#import_crt").show();
		$("#import_crt").css("left", tempX );
				
		if( tempY + $("#filelink").height() > $("body").height() )
			$("#import_crt").css("top", $("body").height() - $("#filelink").height() );
		else
			$("#import_crt").css("top", tempY );
	});
	
	$("button#btn_export_crt").click(function(){
		window.open("/aicloud.crt");
	});
	
	$("button#btn_create_account").click(function(){
		g_create_account_dialog.data("action", "new").dialog("open");
	});	
	
	$("button#btn_create_invite").click(function(){
		g_create_account_dialog.data("action", "invite").dialog("open");
	});
	
	$('li#rescan a').text(m.getString('title_rescan'));
	$('li#sharelink a').text(m.getString('title_sharelink'));
	$('li#version a').text(m.getString('title_version'));
	$('li#settings a').text(m.getString('title_crt'));
	$('li#account a').text(m.getString('title_account'));
	$('button#ok').text(m.getString('btn_ok'));
	$('button#cancel').text(m.getString('btn_close'));
	$('#btn_rescan').text(m.getString('btn_rescan'));
	$('span#desc_rescan').text(m.getString('title_desc_rescan'));
	$('span#aicloud_desc_version').text("AiCloud " + m.getString('title_version'));
	$('span#aicloud_version').text(g_storage.get('aicloud_version'));	
	$('span#smartsync_desc_version').text("SmartSync " + m.getString('title_version'));
	$('span#smartsync_version').text(g_storage.get('smartsync_version'));
	
	$('span#desc_version').text("FW " + m.getString('title_version'));
	$('span#version').text(g_storage.get('router_version'));
	
	$("p.desc_share_link").text(m.getString('title_copy_string'));
	
	$("#label_gen_crt").text(m.getString("title_gen_crt"));
	$("#btn_gen_crt").text(m.getString("title_gen_crt"));
	$("#btn_import_crt").text(m.getString("title_import_crt"));
	$("#desc_gen_crt").text(m.getString("title_desc_gen_crt"));
	$("#label_server_crt").text(m.getString("title_server_crt"));
	$("#label_crt_type").text(m.getString("title_crt_type"));
	$("#label_self_crt_type").text(m.getString("title_self_crt_type"));
	$("#label_crt_to").text(m.getString("title_crt_to"));
	$("#label_crt_from").text(m.getString("title_crt_from"));
	$("#label_crt_end_date").text(m.getString("title_crt_end_date"));
	$("#btn_export_crt").text(m.getString("title_export_crt"));
	$("#label_https_crt_cn").text(m.getString('https_crt_cn'));
	$("#crt_creating").text(m.getString('msg_gen_crt_creating'));
	
	$("#desc_import_crt").text(m.getString('desc_import_crt'));
	$("#label_select_private_key").text(m.getString('title_select_private_key'));
	$("#label_select_certificate").text(m.getString('title_select_certificate'));
	$("#label_select_intermediate_certificate").text(m.getString('title_select_intermediate_certificate'));
	$("#import").text(m.getString('title_import_crt'));
	
	//- account manager
	$("#label_create_account").text(m.getString('title_create_account'));
	$("#desc_create_account").text(m.getString('title_desc_create_account'));
	$("#btn_create_account").text(m.getString('title_create_account'));
	$("#btn_create_invite").text(m.getString('title_invite_account'));
	$("#label_username").text(m.getString('title_username'));
	$("#label_password").text(m.getString('title_password'));
	$("#label_password_confirm").text(m.getString('title_password_confirm'));	
	$("#label_size_limit").text(m.getString('title_size_limit'));
	$("#label_access_permission").text(m.getString('title_access_permission'));
	$("#label_account_list").text(m.getString('title_account_list'));
	$("#table_account_list td[field=username]").text(m.getString('title_username'));
	$("#table_account_list td[field=type]").text(m.getString('title_type'));
	$("#table_account_list td[field=edit]").text(m.getString('title_edit'));
	$("#table_account_list td[field=delete]").text(m.getString('title_delete'));
	$("#label_account_invite_list").text(m.getString('title_account_invite_list'));
	$("#table_account_invite_list td[field=invite]").text(m.getString('title_invite'));
	$("#table_account_invite_list td[field=createtime]").text(m.getString('title_create_time'));
	$("#table_account_invite_list td[field=edit]").text(m.getString('title_edit'));
	$("#table_account_invite_list td[field=delete]").text(m.getString('title_delete'));
	
	$("#label_security_code").text(m.getString('title_security_code'));
	$("#select_security_code option[value=0]").text(m.getString('title_security_code_type_none'));
	$("#select_security_code option[value=1]").text(m.getString('title_security_code_type_manually'));
	$("#select_security_code option[value=2]").text(m.getString('title_security_code_type_auto'));
	
	$("#label_smart_access").text(m.getString('title_smart_access'));
	$("#span_smart_access").text(m.getString('title_enable'));
	
	if(_showTabIndex==1)
		refreshShareLinkList();
	else if(_showTabIndex==3)
		refreshCertificateInfo();
	
	$(".abgne_tab").css("height", $(window).height()-80);
	
	$("#facebook .user_login_option").click(function(){
		
		if(g_bLoginFacebook)
			return;
			
		facebook_login();
	});
	
	$("#google .user_login_option").click(function(){
		
		if(g_bLoginGoogle)
			return;
			
		google_login();
	});
	
	$("#btn_select_key").change(function(evt){
		
		if(evt.target.files.length!=1){
			alert("Please select a key file!");
			return;
		}
		
		g_key_file = evt.target.files[0];
	});
	
	$("#btn_select_crt").change(function(evt){
		if(evt.target.files.length!=1){
			alert("Please select a key file!");
			return;
		}
		
		g_crt_file = evt.target.files[0];
	});
	
	$("#btn_select_intermediate_crt").change(function(evt){
		if(evt.target.files.length!=1){
			return;
		}
		
		g_intermediate_crt_file = evt.target.files[0];
	});
	
	$("#select_security_code").change(function() {
		if($(this).val()==1){
			$("#input_security_code").show();
		}	
		else{
			$("#input_security_code").hide();
		}
	});
	
	g_create_account_dialog = $("#dialog_create_account").dialog({
    	autoOpen: false,
      	height: 500,
      	width: 600,
      	modal: true,
      	buttons: {
        	OK: function(){
        		
        		var username = myencodeURI(String($("#input_username").val()).trim());
        		var password = String($("#input_password").val()).trim();
        		var password_confirm = String($("#input_password_confirm").val()).trim();
        		var type = "aicloud";
        		var action = $(this).data('action');
        		var user_id = -1;
        		
        		if(action!="invite" && action!="modify_invite"){
	        		if(username==""){
	        			//- 帳號不可空白
	        			alert(m.getString('msg_err_account_blank'));
	        			return;	
	        		}
	        		
	        		if(password!=password_confirm){
	        			//- 密碼輸入不正確
	        			alert(m.getString('msg_err_password_matched'));
	        			return;	
	        		}
        		}
        		
        		var client = new davlib.DavClient();
				client.initialize();
				
      			if(action=="modify"){
      				user_id = parseInt($(this).data('id'));
      			}
      			
      			var result_folder_permission = "";
      			for (var key_partion in g_folder_permission_info){
	  				if (g_folder_permission_info.hasOwnProperty(key_partion)) {
	  					for (var key_folder in g_folder_permission_info[key_partion]){
	  						if (g_folder_permission_info[key_partion].hasOwnProperty(key_folder)) {
	  							var folder_permission = g_folder_permission_info[key_partion][key_folder];
	  							result_folder_permission += myencodeURI(key_partion)+","+myencodeURI(key_folder)+","+folder_permission;
	  							result_folder_permission += ";";
	  							//alert(key_partion + " -> " + key_folder + " -> " + folder_permission);
	  						}
	  					}	
	  				}
	  			}
	  			
	  			username = encodeURIComponent(username);
	  			password = encodeURIComponent(password);
	  			type = encodeURIComponent(type);
	  			result_folder_permission = encodeURIComponent(result_folder_permission);
	  			
	  			if(action=="invite" || action=="modify_invite"){
	  				
	  				var security_code = "";
							
					if($("#select_security_code").val()==0){
						security_code = "";
					}
					else if($("#select_security_code").val()==1){
						security_code = $("#input_security_code").val();
						
						if(security_code==""){
							alert("Please input security code!");
							return;
						}
					}
					else if($("#select_security_code").val()==2){
						security_code = Math.floor(1000 + Math.random() * 9000);
					}
					
					var enable_smart_access = ($("#input_smart_access").prop("checked")==true) ? 1 : 0;
					
	  				client.UPDATEACCOUNTINVITE( '/', g_invite_token, result_folder_permission, enable_smart_access, security_code, function(error, statusstring, content){
						if(error==200){
							
							var data = parseXml(content);
							var x = $(data);
			
							var invite_token = x.find("token").text();
							
							g_create_account_dialog.dialog( "close" );
							
							query_account_invite_list();
							
							//- show invite info
							if(action=="invite"){
								g_account_info_dialog
					  				.data("action", "invite")
					  				.data("invite_token", invite_token)
					  				.data("enable_smart_access", enable_smart_access)
					  				.data("security_code", security_code)
					  				.dialog("open");
					  		}
						}
						else{
							//- Fail to create account, please check and try again.
							alert(m.getString("msg_err_account_create"));
						}
					}, null );
				
	  				return;
	  			}
	  			
      			client.UPDATEACCOUNT( '/', user_id, username, password, type, result_folder_permission, function(error, statusstring, content){
					if(error==200){
						if(content=="ACCOUNT_IS_EXISTED"){
							//- Account is already existed!;
							alert(m.getString("msg_err_account_existed"));
							return;
						}
							
						set_account_input_default_val();
	        	
						g_create_account_dialog.dialog( "close" );
							
						query_account_list();
					}
					else{
						//- Fail to create account, please check and try again.
						alert(m.getString("msg_err_account_create"));
					}
				}, null );
      			      			
        	},
        	CANCEL: function() {
          		g_create_account_dialog.dialog( "close" );
        	}
      	},
      	close: function() {
      		
      	},
      	open: function() {
      		
      		$("#dialog_create_account .ui-button-text:contains(OK)").text(m.getString('btn_ok'));
      		$("#dialog_create_account .ui-button-text:contains(CANCEL)").text(m.getString('btn_cancel'));
      		
      		var action = $(this).data('action');
      		
      		if(action=="modify"){
      			var user_id = parseInt($(this).data('id'));
      			var user_name = $(this).data('username');
      			var user_type = $(this).data('usertype');
      			
      			$("#dialog_create_account").dialog("option", "title", m.getString('title_update_account'));
      			$("#input_username").attr("disabled", true);
      			
				var client = new davlib.DavClient();
				client.initialize();
					
				client.GETACCOUNTINFO( '/', user_name, function(context, status, statusstring){
					if(context==200){
						
						var data = parseXml(statusstring);
						var x = $(data);
			
						var username = x.find("username").text();
						var type = x.find("type").text();
						var permission = x.find("permission").text();
						var password = x.find("password").text();
						
						$(".option_account").show();
						$(".option_invite").hide();
						$("#input_username").val(username);
					    $("#input_password").val(password);
					    $("#input_password_confirm").val(password);
					    $("#table_share_folder").css("height", 220);
					    
					    query_partition_share_folder(type, username, g_storage.get('usbdiskname'), g_storage.get('usbdiskname'), "table_share_folder", 0);
					}
				}, null );      			
      		}
      		else if(action=="new"){
      			$("#dialog_create_account").dialog("option", "title", m.getString('title_create_account'));
      			$(".option_account").show();
      			$(".option_invite").hide();
      			$("#input_username").attr("disabled", false);
      			$("#input_username").val("");
				$("#input_password").val("");
				$("#input_password_confirm").val("");
				$("#input_name").val("");
				$("#table_share_folder").css("height", 220);
					    
      			query_partition_share_folder('', '', g_storage.get('usbdiskname'), g_storage.get('usbdiskname'), "table_share_folder", 0);
      		}
      		else if(action=="invite"){
      			$("#dialog_create_account").dialog("option", "title", m.getString('title_invite_account'));
      			$(".option_account").hide();
      			$(".option_invite").show();
      			$("#table_share_folder").css("height", 280);
      			
      			g_invite_token = "";
      			
				query_partition_share_folder('', '', g_storage.get('usbdiskname'), g_storage.get('usbdiskname'), "table_share_folder", 0);
      		}
      		else if(action=="modify_invite"){
      			var token = $(this).data('token');
      			var client = new davlib.DavClient();
				client.initialize();
				
				client.GETACCOUNTINVITEINFO( '/', token, function(context, status, statusstring){
					if(context==200){
						
						var data = parseXml(statusstring);
						var x = $(data);
						
						var permission = x.find("permission").text();
						var smart_access = parseInt(x.find("smart_access").text());
						var security_code = x.find("security_code").text();
						
						g_invite_token = token;
						
						$(".option_account").hide();
						$(".option_invite").show();
					    $("#table_share_folder").css("height", 280);
					    
					    $("#input_smart_access").prop("checked", (smart_access==1) ? true : false);
					    
					    if(security_code==""){
					    	$("#select_security_code").val(0);
					    	$("#input_security_code").hide();
					    }
					    else{
					    	$("#select_security_code").val(1);
					    	$("#input_security_code").show();
					    	$("#input_security_code").val(security_code);
					    }
					    
					    query_partition_share_folder('', '', g_storage.get('usbdiskname'), g_storage.get('usbdiskname'), "table_share_folder", 0);
					}
				}, null );
      		}
      	}
    });
    
    g_account_info_dialog = $("#dialog_account_info").dialog({
    	autoOpen: false,
      	height: 500,
      	width: 600,
      	modal: true,
      	buttons: {
      		OK: function() {
          		g_account_info_dialog.dialog( "close" );
        	}
      	},
      	open: function(){
      		var action = $(this).data('action');
      		var invite_token = $(this).data('invite_token');
      		var enable_smart_access = $(this).data('enable_smart_access');
      		var security_code = $(this).data('security_code');
      		
      		if(action=="invite"){
      			
      			var invite_url = "";
				if(g_storage.get('ddns_host_name')==""){
					var b = window.location.href.indexOf("/",window.location.protocol.length+2);
					invite_url = window.location.href.slice(0,b) + "/" + invite_token;
				}
				else{
					invite_url = "https://" + g_storage.get('ddns_host_name');
					
					if(g_storage.get("https_port")!="443")
						invite_url += ":" + g_storage.get("https_port");
						
					invite_url += "/" + invite_token;
				}
				
				var invitation_desc = m.getString("msg_invitation_desc1");
				var smartaccess_desc = m.getString("title_smart_access") + ": " + ((enable_smart_access==1) ? m.getString("title_enable") : m.getString("title_disable"));
				var securitycode_desc = m.getString("title_security_code") + ": " + ((security_code==""||security_code=="none") ? m.getString("title_security_code_type_none") : security_code);
				
				var account_info = invitation_desc.replace(/\n/g, "<br><br>");
				account_info += "<a style=\"color:#ff0000\" href=\"" + invite_url + "\" target=\"_blank\">" + invite_url + "</a>";
				account_info += "<br><br>" + smartaccess_desc;
				account_info += "<br><br>" + securitycode_desc;
							
				$("#account_info").html(account_info);
				
				var mail_content = "mailto:?subject=Welcome to my AiCloud";
      			mail_content += "&body=";
      			mail_content += encodeURIComponent(invitation_desc); 
      			mail_content += encodeURIComponent(invite_url);
      			mail_content += encodeURIComponent("\n" + smartaccess_desc);
      			mail_content += encodeURIComponent("\n" + securitycode_desc);
      			
				$("#mail_invitation").attr("href", mail_content);
      		}
      	}
    });
    
    query_account_list();
    query_account_invite_list();
});

var g_folder_permission_info = Array();

function query_partition_share_folder(account_type, account, path, folder, table_name, level){
	
	if(level>1)
		return;
		
	var param = {};
    param.action = "query_disk_folder";
    param.id = "#";
    param.path = path;
    param.type = account_type;
    param.account = account;
    
    $.ajax({
		url: '/query_field.json',
	  	data: param,
	  	type: 'GET',
	  	dataType: 'json',
	  	timeout: 20000,
	  	error: function(){
			alert('Error loading json data!');
	  	},
	  	success: function(json_data){
	  		var html_data = "<table class='tree_table'>";
	  		
	  		level+=1;
	  		  
	  		for(var i=0; i<json_data.length; i++){
	  			var folder_name = json_data[i].text;
	  			var folder_permission = parseInt(json_data[i].permission);
	  			var random_seed = Math.random().toString(36).replace(/[^a-z]+/g, '').substr(0, 5);
	  			var the_id = random_seed + "_" + i;
	  			  				  			
	  			if(level==1){		
	  				html_data += "<tr class='partion_table_tr' style='background-color:#989898;color:#ffffff'>";
	  				html_data += "<td>";
	  				html_data += folder_name;
	  				html_data += "</td>";
	  				html_data += "<td></td>";
	  				html_data += "</tr>";
	  				
	  				g_folder_permission_info[folder_name] = Array();
	  			}
	  			else if(level==2){
	  				html_data += "<tr class='folder_table_tr' style='border-bottom:1px solid #989898'>";
	  				
	  				html_data += "<td style='padding-left:10px'>";
		  			html_data += folder_name;
		  			html_data += "</td>";
	  			
	  				html_data += "<td style='width:150px;'>";
	  				html_data += "<div id='f" + the_id + "' data-id='" + the_id + "' class='FileStatus' data-partition='" + folder + "' data-folder='" + folder_name + "'>";
	  				html_data += "<table>";
	  				html_data += "<tr data-permission='" + folder_permission + "'>";
	  				html_data += "<td><input type='radio' class='radio_permission' name='g" + the_id + "' id='g" + the_id + "' value='3'" + ((folder_permission==3) ? " checked" : "") + ">R/W</td>";
					html_data += "<td><input type='radio' class='radio_permission' name='g" + the_id + "' id='g" + the_id + "' value='1'" + ((folder_permission==1) ? " checked" : "") + ">R</td>";
					html_data += "<td><input type='radio' class='radio_permission' name='g" + the_id + "' id='g" + the_id + "' value='0'" + ((folder_permission==0||folder_permission==-1) ? " checked" : "") + ">N</td>";
					html_data += "</tr>";
					html_data += "</table>";
					html_data += "</div>";
					html_data += "</td>";
					
					html_data += "</tr>";
					
					g_folder_permission_info[folder][folder_name] = folder_permission;
				}
	  			
	  			var sub_table_name = "table_" + random_seed;
	  			html_data += "<tr><td colspan='2'><div id='" + sub_table_name + "'></div></td></tr>";
	  			query_partition_share_folder(account_type, account, path + "/" + folder_name, folder_name, sub_table_name, level);
	  		}
	  		
	  		html_data += "</table>";
	  		
	  		$("#"+ table_name).html(html_data);
	  		
	  		$(".folder_table_tr").mouseenter(function(){
	  			$(this).css("background-color", "#cccccc");
	  		});
	  		
	  		$(".folder_table_tr").mouseleave(function(){
	  			$(this).css("background-color", "transparent");
	  		});
	  		
	  		$(".radio_permission").change(function(){
	  			var data_id = $(this).closest('.FileStatus').attr("data-id");
	  			var partion_name = $(this).closest('.FileStatus').attr("data-partition");
	  			var folder_name = $(this).closest('.FileStatus').attr("data-folder");
	  			var folder_permission = parseInt($(this).val());
	  			
	  			g_folder_permission_info[partion_name][folder_name] = folder_permission;
	  			
	  			//console.log(g_folder_permission_info);
	  		});
	 	}
	});
}

function set_account_input_default_val(){
	$("#input_username").val("");
    $("#input_password").val("");
    $("#input_password_confirm").val("");
    $("#input_name").val("");
}

function query_account_list(){
	
	var client = new davlib.DavClient();
	client.initialize();
		
	client.GETACCOUNTLIST( '/', function(context, status, statusstring){
		if(context==200){
			
			var data = parseXml(statusstring);
			var x = $(data);
			
			var account_list = x.find("account");
			
			var html_data = "";
	  		
	  		for(var i=0; i<account_list.length; i++){
	  			
	  			var data_account = parseXml(account_list[i]);
				var y = $(data_account);
				
	  			var id = y.find("id").text();
	  			var username = y.find("username").text();
	  			var type = y.find("type").text();
	  			
	  			html_data += "<tr data-acid='" + id + "' data-username='" + username + "' data-usertype='" + type + "'>";
	  			html_data += "<td>";
	  			html_data += username;
	  			html_data += "</td>";
	  			html_data += "<td>";
	  			html_data += type;
	  			html_data += "</td>";
	  			html_data += "<td align='center'>";
	  			
	  			if(type=="aicloud")
	  				html_data += "<input class='edit_account' type='button' value='" + m.getString('title_edit') + "'/>";
	  				
	  			html_data += "</td>";
	  			html_data += "<td align='center'>";
	  			
	  			if(type=="aicloud")
	  				html_data += "<input class='delete_account' type='button' value='" + m.getString('title_delete') + "'/>";
	  				
	  			html_data += "</td>";
	  			html_data += "</tr>";
	  		}
	  		
	  		
	  		$("#table_account_list tbody").empty();
	  		$("#table_account_list tbody").append(html_data);
	  		
	  		$(".edit_account").click(function(){
	  			var item_info = $(this).closest("tr");
	  			var account_id = item_info.attr("data-acid");
	  			var account_username = item_info.attr("data-username");
	  			var account_usertype = item_info.attr("data-usertype");
	  			
	  			g_create_account_dialog
	  				.data("action", "modify")
	  				.data("id", account_id)
	  				.data("username", account_username)
	  				.data("usertype", account_usertype)
	  				.dialog("open"); 
	  		});
	  		
	  		$(".delete_account").click(function(){
	  			var item_info = $(this).closest("tr");
	  			var account_id = item_info.attr("data-acid");
	  			var account_username = item_info.attr("data-username");
	  			var msg = m.getString("msg_delete_account");
				msg = msg.replace("%s", account_username);
		
	  			if(confirm(msg)){
		  			var client = new davlib.DavClient();
					client.initialize();
					client.DELETEACCOUNT( '/', account_username, function(error, statusstring, content){
						if(error==200){
							query_account_list();
						}
						else{
							alert("Fail to delete account.");
						}
					}, null );
				}
	  		});
		}
		
	}, null );
	
}

function query_account_invite_list(){
	
	var b = window.location.href.indexOf("/",window.location.protocol.length+2);
	var window_url = window.location.href.slice(0,b);
	
	var window_url = "";
	if(g_storage.get('ddns_host_name')==""){
		var b = window.location.href.indexOf("/",window.location.protocol.length+2);
		window_url = window.location.href.slice(0,b);
	}
	else{
		window_url = "https://" + g_storage.get('ddns_host_name');
	}
							
	var client = new davlib.DavClient();
	client.initialize();
	
	client.GETACCOUNTINVITELIST( '/', function(context, status, statusstring){
		if(context==200){
			
			var data = parseXml(statusstring);
			var x = $(data);
			
			var invite_list = x.find("invite");
			
			var html_data = "";
	  		
	  		for(var i=0; i<invite_list.length; i++){
	  			
	  			var data_invite = parseXml(invite_list[i]);
				var y = $(data_invite);
				
	  			var id = y.find("id").text();
	  			var token = y.find("token").text();
	  			var timestamp = y.find("timestamp").text();
	  			var smart_access = y.find("smart_access").text();
	  			var security_code = y.find("security_code").text();
	  			
	  			html_data += "<tr data-aciid='" + id + "' data-token='" + token + "' data-smart-access='" + smart_access + "' data-security-code='" + security_code + "'>";
	  			html_data += "<td>";
	  			html_data += "<a href='#' class='show_account_invite_info'>"
	  			html_data += token;
	  			html_data += "</a>";
	  			html_data += "</td>";
	  			html_data += "<td>";
	  			html_data += timestamp;
	  			html_data += "</td>";
	  			html_data += "<td align='center'>";
	  			
	  			html_data += "<input class='edit_account_invite' type='button' value='" + m.getString('title_edit') + "'/>";
	  				
	  			html_data += "</td>";
	  			html_data += "<td align='center'>";
	  			
	  			html_data += "<input class='delete_account_invite' type='button' value='" + m.getString('title_delete') + "'/>";
	  				
	  			html_data += "</td>";
	  			html_data += "</tr>";
	  		}
	  		
	  		
	  		$("#table_account_invite_list tbody").empty();
	  		$("#table_account_invite_list tbody").append(html_data);
	  		
	  		$(".edit_account_invite").click(function(){
	  			var item_info = $(this).closest("tr");
	  			var invite_id = item_info.attr("data-aciid");
	  			var invite_token = item_info.attr("data-token");
	  			
	  			g_create_account_dialog
	  				.data("action", "modify_invite")	  				
	  				.data("token", invite_token)	  				
	  				.dialog("open");	  			 
	  		});
	  		
	  		$(".delete_account_invite").click(function(){
	  			var item_info = $(this).closest("tr");
	  			var invite_id = item_info.attr("data-aciid");
	  			var invite_token = item_info.attr("data-token");
	  			var msg = m.getString("msg_delete_account_invite");
				
	  			if(confirm(msg)){
		  			var client = new davlib.DavClient();
					client.initialize();
					client.DELETEACCOUNTINVITE( '/', invite_token, function(error, statusstring, content){
						if(error==200){
							query_account_invite_list();
						}
						else{
							alert("Fail to delete account invite.");
						}
					}, null );
				}
	  		});
	  		
	  		$(".show_account_invite_info").click(function(){
	  			var item_info = $(this).closest("tr");
	  			var invite_id = item_info.attr("data-aciid");
	  			var invite_token = item_info.attr("data-token");
	  			var enable_smart_access = item_info.attr("data-smart-access");
	  			var security_code = item_info.attr("data-security-code");
	  			
	  			//- show invite info
				g_account_info_dialog
					.data("action", "invite")
				  	.data("invite_token", invite_token)
				  	.data("enable_smart_access", enable_smart_access)
				  	.data("security_code", security_code)
				  	.dialog("open");
	  		});
		}
		
	}, null );
	
}

function onFacebookLogin( token, uid ){
	
	alert(token+", "+uid);
	
	/*
	g_token = token,
	g_uid = uid;
	
	g_storage.sett("facebook_access_token", g_token);
	g_storage.sett("facebook_uid", uid);
	
	facebook_get_userprofile();
	*/
}

function facebook_login(){
	var app_id = "697618710295679";
	window._onFacebookLogin = this.onFacebookLogin;
	var b = window.location.href.indexOf("/",window.location.protocol.length+2);
	var callback_function = "_onFacebookLogin";
	
	var auth_url = server_url + "fb_authorize.html?callback=" + callback_function + "&ps5host=";
	var redirect_url = window.location.href.slice(0,b) + "/smb/css/service/callback.html&response_type=token";
	var url = "http://www.facebook.com/dialog/oauth/?scope=publish_stream%2Cuser_photos%2Coffline_access&client_id=" + app_id + "&display=popup&redirect_uri=" + encodeURIComponent(auth_url) + redirect_url;
	
	window.open(url,"mywindow","menubar=1,resizable=0,width=630,height=250, top=100, left=300");
}

function onGoogleLogin( token, uid ){
	
	alert(token+", "+uid);
	
	/*
	g_token = token,
	g_uid = uid;
	
	g_storage.sett("facebook_access_token", g_token);
	g_storage.sett("facebook_uid", uid);
	
	facebook_get_userprofile();
	*/
}

function google_login(){
	var app_id = "103584452676-oo7gkbh8dg7nm07lao9a0i3r9jh6jfra.apps.googleusercontent.com";
	var b = window.location.href.indexOf("/",window.location.protocol.length+2);
	var callback_function = "onGoogleLogin";
		
	var auth_url = server_url + "google_authorize.html";
	var redirect_url = window.location.href.slice(0,b) + "/smb/css/service/callback.html";
		
	var url = "https://accounts.google.com/o/oauth2/auth?" +
			  "response_type=token" +				  
			  "&redirect_uri=" + encodeURIComponent(auth_url) + 
			  "&client_id=" + app_id +
			  "&scope=https://www.googleapis.com%2Fauth%2Fuserinfo.email+https://www.googleapis.com%2Fauth%2Fuserinfo.profile+https://picasaweb.google.com%2Fdata" + 
			  "&state=/callback=" + callback_function + "+ps5host=" + redirect_url;
		
	window.open(url,"mywindow","menubar=1,resizable=0,width=630,height=250, top=100, left=300");
}

function getLatestVersion(){
	var client = new davlib.DavClient();
	client.initialize();
	
	$("#update").text(m.getString('msg_check_latest_ver'));
	
	client.GETLATESTVER("/", function(error, content, statusstring){	
		if(error==200){
			var data = parseXml(statusstring);
			var x = $(data);
			
			var ver = x.find("version").text();
			var a = ver.split("_");
			var build_no = a[1];
			
			var cur_ver = g_storage.get('router_version');
			var b = cur_ver.split(".");
			var cur_build_no = b[3];
			
			if(build_no>cur_build_no)
				$("#update").text(m.getString('msg_update_latest_ver'));
			else
				$("#update").text(m.getString('msg_latest_ver'));
		}
		else{
			$("#update").text(m.getString('msg_check_latest_ver_error'));
		}
	
		client = null;
	});
}

function refreshShareLinkList(){
	var webdav_mode = g_storage.get('webdav_mode');
	var ddns_host_name = g_storage.get('ddns_host_name');    
	var cur_host_name = window.location.host;
	var hostName = "";
				
	if(!isPrivateIP(cur_host_name))
		hostName = cur_host_name;
	else			
		hostName = (ddns_host_name=="") ? cur_host_name : ddns_host_name;
		
	if(hostName.indexOf(":")!=-1)
		hostName = hostName.substring(0, hostName.indexOf(":"));
			
	if( webdav_mode == 0 ) //- Only enable http
		hostName = "http://" + hostName + ":" + g_storage.get("http_port");
	else{
		hostName = "https://" + hostName;
					
		if(g_storage.get("https_port")!="443")
			hostName += ":" + g_storage.get("https_port");
	}
	
	var client = new davlib.DavClient();
	client.initialize();
	client.GSLL("/", function(error, content, statusstring){	
		if(error==200){
			var data = parseXml(statusstring);
			
			$("#tab2").empty();
			
			var table_html = "<table id='sharelink' width='100%' border='0' style='table-layout:fixed'>";
			table_html += "<thead><tr>";
     		table_html += "<th scope='col' class='check' style='width:5%'>";
			table_html += "<input type='checkbox' id='select_all' name='select_all' class='select_all'>";
			table_html += "</th>";
			
			table_html += "<th scope='col' class='filename' style='width:20%'>" + m.getString('table_filename') + "</th>";
      		table_html += "<th scope='col' class='createtime' style='width:25%'>" + m.getString('table_createtime') + "</th>";
      		table_html += "<th scope='col' class='expiretime' style='width:25%'>" + m.getString('table_expiretime') + "</th>";
      		table_html += "<th scope='col' class='lefttime' style='width:15%'>" + m.getString('table_lefttime') + "</th>";
      		table_html += "<th scope='col' class='remove' style='width:10%'>" + m.getString('func_delete') + "</th>";
    		table_html += "</tr></thead>";
    	
    		table_html += "<tbody id='ntb'>";
    		
			var encode_filename = parseInt($(data).find('encode_filename').text());
			
    		var i = 0;
			$(data).find('sharelink').each(function(){
				
				try{
					var filename = "";
					var filetitle = "";
					
					if(encode_filename==1){
						filename = $(this).attr("filename");				
						filetitle = decodeURIComponent(filename);
					}
					else{
						filetitle = $(this).attr("filename");				
						filename = encodeURIComponent($(this).attr("filename"));
					}
						
					var url = hostName + "/" + $(this).attr("url") + "/" + filename;
					var createtime = $(this).attr("createtime");
					var expiretime = $(this).attr("expiretime");
					var lefttime = parseFloat($(this).attr("lefttime"));
					var hour = parseInt(lefttime/3600);
					var minute = parseInt(lefttime%3600/60);
													
					table_html += "<tr nid='" + i + "' class='even'>";
					
					table_html += "<td fid='check' align='center'><input type='checkbox' id='check_del' name='check_del' class='check_del' link=\"" + $(this).attr("url") + "\"></td>";
					
					table_html += "<td fid='filename' align='center'><div style='overflow:hidden;'>";
					table_html += "<a class='share_link_url' uhref=\"" + url + "\" href='#' title=\"" + filetitle + "\" style='white-space:nowrap;'>" + filetitle + "</a>";
					table_html += "</div></td>";
					table_html += "<td fid='createtime' align='center'>" + createtime + "</td>";
					if(expiretime==0){
						table_html += "<td fid='expiretime' align='center'>" + m.getString('title_unlimited') + "</td>";
						table_html += "<td fid='lefttime' align='center'>" + m.getString('title_unlimited') + "</td>";
					}
					else{
						table_html += "<td fid='expiretime' align='center'>" + expiretime + "</td>";
						table_html += "<td fid='lefttime' align='center'>" + hour + " hours " + minute + " mins" + "</td>";
					}
					table_html += "<td fid='remove' align='center'><a>";
					table_html += "<div class='dellink' title='remove' link=\"" + $(this).attr("url") + "\" style='cursor:pointer'></div>";
					table_html += "</a></td>";
					
					table_html += "</tr>";
			  	}
				catch(err){
					//Handle errors here				  	
				}
				
				i++;
			});
			
			table_html += "</tbody>";      
			table_html += "</table>";
			
			table_html += "<div class='delcheck_block'>";
			table_html += "<span>刪除選取連結</span>";
			table_html += "</div>";
			
			$(table_html).appendTo($("#tab2"));
			
			$("div.delcheck_block").css("visibility", "hidden");	
			
			$("a.share_link_url").click(function(){
				$("#filelink").css("display","block");
				$("#filelink").css("left", tempX );
				
				if( tempY + $("#filelink").height() > $("body").height() )
					$("#filelink").css("top", $("body").height() - $("#filelink").height() );
				else
					$("#filelink").css("top", tempY );
				
				$("#resourcefile").attr("value",$(this).attr("uhref"));
				$("#resourcefile").focus();
				$("#resourcefile").select();
			});
			
			$(".dellink").click(function(){
				
				var r=confirm(m.getString('msg_confirm_delete_sharelink'));
				
				if (r==true){
					client.REMOVESL("/", $(this).attr("link"), function(error, content, statusstring){	
						if(error==200){
							refreshShareLinkList();
						}
					});
				}
			});
			
			$(".check_del").change(function(){
				
				var del_count = 0;
		
				$("input:checkbox.check_del").each(function(){
					if($(this).prop("checked")){
						del_count++;
					}
				});
				
				if(del_count<=0){
										
					var newTop = tempY+10;
					var newLeft = 0;
					$("div.delcheck_block").animate({
						top: newTop,
						left: newLeft
					}, 
					'fast', 
					function(){
						$("div.delcheck_block").css("visibility", "hidden");	
					});
				}
				else{
					
					$("div.delcheck_block").css("visibility", "");
					
					var newTop = tempY+10;
					var newLeft = tempX+10;
					$("div.delcheck_block").animate({
						top: newTop,
						left: newLeft
					}, 'fast');
				}
			});
			
			$(".delcheck_block").click(function(){
				
				var r=confirm(m.getString('msg_confirm_delete_sharelink'));
				
				if (r==true){
					
					$("div.delcheck_block").css("visibility", "hidden");
					
					$("input:checkbox.check_del").each(function(){
						if($(this).prop("checked")){
							
							client.REMOVESL("/", $(this).attr("link"), function(error, content, statusstring){	
								if(error==200){
									refreshShareLinkList();
								}
							});
						}
					});
				}				
			});
			
			$("input.select_all").change(function(){
				if($(this).prop("checked")){
					$('input:checkbox.check_del').prop('checked', true);					
					$("div.delcheck_block").css("visibility", "");
					
					var newTop = tempY+10;
					var newLeft = tempX+10;
					$("div.delcheck_block").animate({
						top: newTop,
						left: newLeft
					}, 'fast');
				}
				else{
					$('input:checkbox.check_del').prop('checked', false);
					
					var newTop = tempY+10;
					var newLeft = 0;
					$("div.delcheck_block").animate({
						top: newTop,
						left: newLeft
					}, 
					'fast', 
					function(){
						$("div.delcheck_block").css("visibility", "hidden");	
					});
				}
			});
		}
	});
}

function refreshCertificateInfo(){
	
	var userpermission = (g_storage.get('userpermission')==undefined) ? "" : g_storage.get('userpermission');
	if(userpermission=="admin"){
		$("#field_import_crt").show();
	}
	else{
		$("#field_import_crt").hide();
	}
		
	var client = new davlib.DavClient();
	client.initialize();
	client.GETX509CERTINFO("/", function(error, content, statusstring){	
		if(error==200){
				
			var data = parseXml(statusstring);
						
			var issuer = $(data).find('issuer').text();
			var subject = $(data).find('subject').text();
			var crt_start_date = $(data).find('crt_start_date').text();
			var crt_end_date = $(data).find('crt_end_date').text();
				
			if(subject!=""){
				subject = subject.substring( subject.indexOf("CN=")+3, subject.length );
					
				if(subject.indexOf("/")!=-1){
					subject = subject.substring( 0, subject.indexOf("/") );
				}
			}
				
			if(issuer!=""){
				issuer = issuer.substring( issuer.indexOf("CN=")+3, issuer.length );
					
				if(issuer.indexOf("/")!=-1){
					issuer = issuer.substring( 0, issuer.indexOf("/") );
				}
			}
				
			//crt_end_date = "160607080028Z";
			if(crt_end_date!=""){
				
				var mydate = new Date( 2000+parseInt(crt_end_date.substring(0, 2)), 
				                       parseInt(crt_end_date.substring(2, 4)),
				                       parseInt(crt_end_date.substring(4, 6)));
					                       
				crt_end_date = mydate.getFullYear() + "-" + mydate.getMonth() + "-" + mydate.getDate();
					                       
				//alert(crt_end_date.substring(0, 2)+", "+crt_end_date.substring(2, 4) + ", " + crt_end_date.substring(4, 6));
				//alert(mydate.toISOString().slice(0, 10).replace(/-/g, '-'));
				//alert(mydate.getFullYear() + " - " + mydate.getMonth() + " - " + mydate.getDate());
			}
				
			if(issuer==subject){
				$("#label_self_crt_type").text(m.getString("title_self_crt_type"));
			}
			else{
				$("#label_self_crt_type").text(m.getString("title_third_crt_type"));
			}
				
			$("#label_https_crt_cn").text(subject);
			$("#label_https_crt_issuer").text(issuer);
			$("#label_https_crt_end_date").text(crt_end_date);
			//alert(content + ", " + statusstring);
			
			if(issuer==""){
				$("#btn_export_crt").attr("disabled", true);
			}
			else{
				$("#btn_export_crt").attr("disabled", false);
			}
		}
			
		client = null;	
	});
}

function doOK(e) {
	parent.closeJqmWindow();
};

function doCancel(e) {
	parent.closeJqmWindow();
};

function onCloseShareLink(){
	$("#filelink").css("display","none");	
}

function onCloseImportCrt(){
	$('#import_crt').hide();
}

function read_file(the_file, onReadCompleteHandler){
	if(the_file==null)
		return;
		
	var start = 0;
	var stop = the_file.size-1;
	var bSliceAsBinary = 0;
	var reader = new FileReader();
	reader.onloadend = function(evt) {			
		if (evt.target.readyState == FileReader.DONE) { // DONE == 2
			if(onReadCompleteHandler)
				onReadCompleteHandler(evt, bSliceAsBinary);
		}
	};
			
	if (the_file.webkitSlice) {
		var blob = the_file.webkitSlice(start, stop+1);
		//reader.readAsArrayBuffer(blob);
		reader.readAsText(blob);
		bSliceAsBinary = 1;
	} 
	else if (the_file.mozSlice) {      
		var blob = the_file.mozSlice(start, stop+1);
		reader.readAsBinaryString(blob);
		bSliceAsBinary = 0;			
	}
	else {
		var blob = the_file.slice(start, stop+1);			
		//reader.readAsArrayBuffer(blob);
		reader.readAsText(blob);
		bSliceAsBinary = 1;			
    }
}

function onDoImportCrt(){
	
	if(g_key_file==null){
		alert("Please select a key file.");
		return;
	}
	
	if(g_crt_file==null){
		alert("Please select a crt file.");
		return;
	}
	
	var key_pem = "";
	var crt_pem = "";
	var intermediate_crt_pem = "";
	var set_pem_content = function(){
		
		if(g_key_file!=null && key_pem=="")
			return;
			
		if(g_crt_file!=null && crt_pem=="")
			return;
			
		if(g_intermediate_crt_file!=null && intermediate_crt_pem=="")
			return;
			
		var client = new davlib.DavClient();
		client.initialize();
		
		client.SETROOTCERTIFICATE( '/', key_pem, crt_pem, intermediate_crt_pem, function(context, status, statusstring){
			if(context=="200"){
				$('#import_crt').hide();
				
				alert(m.getString("msg_import_crt_complete"));
					
				client.APPLYAPP("/", "apply", "", "restart_webdav", function(error, statusstring, content){				
					if(error==200){
						setTimeout( function(){
							parent.location.reload(false);
						}, 6000 );
					}
						
					client = null;
				});		
			}
			else{
				alert("Fail to import certificate file! Maybe the imported file is invalid, please check and try again.");
			}
		}, null );
	}
		
	read_file(g_key_file, function(evt, bSliceAsBinary){
		//alert("done: " +　evt.target.result.byteLength + ", " + bSliceAsBinary + ", " + evt.target.result);
		var content = evt.target.result;
		/*
		if(content.indexOf("-----BEGIN RSA PRIVATE KEY-----")!=0){
			alert("Invalid key file format!");
			return;
		}
		*/
		key_pem = content;
		
		set_pem_content();
	});
	
	read_file(g_crt_file, function(evt, bSliceAsBinary){
		var content = evt.target.result;
		/*
		if(content.indexOf("-----BEGIN CERTIFICATE-----")!=0){
			alert("Invalid cert file format!");
			return;
		}
		*/
		crt_pem = content;
		
		set_pem_content();
	});
	
	read_file(g_intermediate_crt_file, function(evt, bSliceAsBinary){
		var content = evt.target.result;
		/*
		if(content.indexOf("-----BEGIN CERTIFICATE-----")!=0){
			alert("Invalid intermediate cert file format!");
			return;
		}
		*/
		intermediate_crt_pem = content;
				
		set_pem_content();
	});
}

// Detect if the browser is IE or not.
// If it is not IE, we assume that the browser is NS.
var IE = document.all?true:false

// If NS -- that is, !IE -- then set up for mouse capture
if (!IE) document.captureEvents(Event.MOUSEMOVE)

// Set-up to use getMouseXY function onMouseMove
document.onmousemove = getMouseXY;

// Temporary variables to hold mouse x-y pos.s
var tempX = 0
var tempY = 0

// Main function to retrieve mouse x-y pos.s

function getMouseXY(e) {
	if (IE) { // grab the x-y pos.s if browser is IE
    	tempX = event.clientX + document.body.scrollLeft
    	tempY = event.clientY + document.body.scrollTop
  	} else {  // grab the x-y pos.s if browser is NS
    	tempX = e.pageX
    	tempY = e.pageY
  	}  
  	// catch possible negative values in NS4
  	if (tempX < 0){tempX = 0}
  	if (tempY < 0){tempY = 0}  
  	return true
}