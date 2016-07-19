function confirm_asus(content){
	/* default config:
		content = {
			title: "Logout Confirm", 
			contentA: "#JS_logout#",
			contentB: "#sub_title#",
			contentC: "#note to user before click button#"
			left_button: "Cancel",
			left_button_callback: function(){},
			left_button_args: {},
			right_button: "Yes",
			right_button_callback: function(){},
			right_button_args: {},
			note: "",
			iframe: "",
			margin: "",
			note_display_flag: 0:stable only, 1:beta only
		}
	*/
	var code = "";
	var release_note_code = "";
	code += '<div class="confirm_block"';
	code += 'style="position:absolute;margin:'+content.margin+';"';
	code += '>';
	code += '<div class="confirm">';
	code += '<div class="confirm_title">' + content.title + '</div>';
	code += '<div class="confirm_contentA">' + content.contentA +'</div>';
	
	if(content.iframe.search("get_release_note") >= 0){
		var webs_state_info_version = webs_state_info.splice(3,0,".").splice(2,0,".").splice(1,0,".");
		var webs_state_info_version_beta = webs_state_info_beta.splice(3,0,".").splice(2,0,".").splice(1,0,".");
		if(content.note_display_flag==0){
			release_note_code += '<div style="display:table;width:93%" class="confirm_contentB">';
			release_note_code += '<#General_x_FirmwareVersion_itemname#> <span style="font-weight:bold; color:#FC0;">'+webs_state_info_version+'</span>';
			release_note_code += '<div style="text-decoration:underline;">Release note</div>';
			release_note_code += '<iframe id="status_iframe" src="'+ content.iframe +'" width="100%" marginwidth="0" marginheight="0" scrolling="no" align="center"></iframe>';
			release_note_code += '</div>';
		}
		else if(content.note_display_flag==1){
			release_note_code += '<div style="display:table;width:93%" class="confirm_contentB">';
			release_note_code += '<#General_x_FirmwareVersion_itemname#> <span style="font-weight:bold; color:#FC0;">'+webs_state_info_version_beta+'</span>';
			release_note_code += '<div style="text-decoration:underline;">Release note</div>';
			release_note_code += '<iframe id="status_iframe" src="'+ content.iframe +'" width="100%" marginwidth="0" marginheight="0" scrolling="no" align="center"></iframe>';
			release_note_code += '</div>';
		}
		code += release_note_code;
		code += '<div class="confirm_contentC">' + content.contentC +'</div>';
	}
	code += '<div style="display:table;width:100%">';
	code += '<div class="confirm_button_gen_long_left">' + content.left_button + '</div>';	//confirm_button confirm_button_left
	code += '<div class="confirm_button_gen_long_right">' + content.right_button + '</div>';	//confirm_button confirm_button_right
	code += '</div>';		
	code += '</div></div>';
	
	$('body').append(code);	
	$('.confirm_button_gen_long_left').bind("click", function(){return content.left_button_callback.call(content.left_button_args)});
	$('.confirm_button_gen_long_right').bind("click", function(){return content.right_button_callback.call(content.right_button_args)});
}

function confirm_cancel(){
	$(".confirm_block").remove();
}
