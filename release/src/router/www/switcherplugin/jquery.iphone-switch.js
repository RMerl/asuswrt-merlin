jQuery.fn.iphoneSwitch = function(start_state, switched_on_callback, switched_off_callback, options) {
	var state = start_state == '1' ? start_state : '0';

	// define default settings
	var settings = {
		mouse_over: 'pointer',
		mouse_out:  'default',
		switch_container_path: '/switcherplugin/iphone_switch_container_on.png',
		switch_path: '/switcherplugin/iphone_switch.png',
		switch_height: 32,
		switch_width: 74
	};

	if(options) {
		jQuery.extend(settings, options);
	}

	// create the switch
	return this.each(function() {

		var container;
		var image;
		
		// make the container
		container = '<div class="iphone_switch_container" style="height:'+settings.switch_height+'px; width:'+settings.switch_width+'px; position: relative; overflow: hidden">';
		
		// make the switch image based on starting state
		image = '<img id="iphone_switch" class="iphone_switch" src="'+settings.switch_container_path+'" style="height:'+settings.switch_height+'px; width:'+settings.switch_width+'px; background-image:url('+settings.switch_path+'); background-repeat:no-repeat; background-position:'+(state == '1' ? 0 : -37)+'px" /></div>';

		// insert into placeholder
		jQuery(this).html(container + image);

		jQuery(this).mouseover(function(){
			jQuery(this).css("cursor", settings.mouse_over);
		});

		jQuery(this).mouseout(function(){
			jQuery(this).css("background", settings.mouse_out);
		});

		// click handling
		jQuery(this).unbind("click"); // initial click event
		jQuery(this).click(function() {
			if((this.id == "radio_clouddisk_enable" || this.id == "radio_web_restrict_enable" || this.id == "apps_analysis_enable" || this.id == "radio_wps_enable" || this.id == "nm_radio_dualwan_enable" || this.id == "simdetect_switch") && typeof(curState))
				state = curState;
			else if(this.id.length > 18 && this.id.substr(0, 18) == "wtfast_rule_enable"){
				var index = (this.id).substr(18);
				var index_int = parseInt(index);
				state = rule_enable_array[index_int];
			}

			if(state == '1') {
				jQuery(this).find('.iphone_switch').animate({backgroundPosition: -37}, "slow", function() {
					jQuery(this).attr('src', settings.switch_container_path);
					if(typeof(index))
						switched_off_callback(index);
					else
						switched_off_callback();
				});
				state = '0';
			}
			else {
				jQuery(this).find('.iphone_switch').animate({backgroundPosition: 0}, "slow", function() {
					jQuery(this).find('.iphone_switch').attr('src', settings.switch_container_path);
					if(typeof(index))
						switched_on_callback(index);
					else
						switched_on_callback();
				});
				state = '1';
			}
		});		

	});	
	
};
