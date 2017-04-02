function wl_chanspec_list_change(){
	var phytype = "n";
	var band = document.form.wl_unit.value;
	var bw_cap = document.form.wl_bw.value;
	var bw_cap_ori = '<% nvram_get("wl_bw"); %>';
	var chanspecs = new Array(0);
	var chanspecs_string = new Array(0);
	var cur = 0;
	var sel = 0;
	var cur_control_channel = 0;
	var extend_channel = new Array();
	var cur_extend_channel = 0;			//current extension channel
	var channel_ori = '<% nvram_get("wl_chanspec"); %>';
	
	if(country == ""){
		country = prompt("The Country Code is not exist! Please enter Country Code.", "");
	}

	/* Save current chanspec */
	cur = '<% nvram_get("wl_chanspec"); %>';
	if (phytype == "a") {	// a mode
		chanspecs = new Array(0); 
	}
	else if (phytype == "n") { // n mode
		if (band == "1") { // ---- 5 GHz
				if(wl_channel_list_5g instanceof Array && wl_channel_list_5g != ["0"]){	//With wireless channel 5g hook or return not ["0"]
						if(based_modelid == "RT-AC3200"){
							wl_channel_list_5g = new Array("36","40","44","48");
						}else if(based_modelid == "RT-AC87U"){
							if(document.form.wl_bw.value==1){
								wl_channel_list_5g = eval('<% channel_list_5g_20m(); %>');
							}else if(document.form.wl_bw.value==2){
								wl_channel_list_5g = eval('<% channel_list_5g_40m(); %>');
							}else  if(document.form.wl_bw.value==3){
								wl_channel_list_5g = eval('<% channel_list_5g_80m(); %>');
							}else{
								wl_channel_list_5g = eval('<% channel_list_5g(); %>');
							}
						}else
							wl_channel_list_5g = eval('<% channel_list_5g(); %>');
					
					extend_channel = ["<#Auto#>"];		 // for 5GHz, extension channel always displays Auto
					extend_channel_value = [""];				
						if (bw_cap != "0" && bw_cap != "1" && wl_channel_list_5g.getIndexByValue("165") >= 0 ) // rm 165, If not [20 MHz] or not [Auto]
								wl_channel_list_5g.splice(wl_channel_list_5g.getIndexByValue("165"),1);
						
						if(bw_cap == "0"){	// [20/40/80 MHz] (auto)
							document.getElementById('wl_nctrlsb_field').style.display = "";
								for(var i=0;i<wl_channel_list_5g.length;i++){
										if(wl_channel_list_5g[i] == "165" || wl_channel_list_5g[i] == "140")		//140, 165 belong to 20MHz
											wl_channel_list_5g[i] = wl_channel_list_5g[i];
										else if((wl_channel_list_5g[i] == "56") && country == "TW")		//56 belong 20MHz only for TW
											wl_channel_list_5g[i] = wl_channel_list_5g[i];										
										else if(band5g_11ac_support){
												if((country == "EU" || country == "E0") && parseInt(wl_channel_list_5g[i]) == 116){		//	belong to 20MHz
													wl_channel_list_5g[i] = wl_channel_list_5g[i];
												}
												else if((country == "EU" || country == "E0") && parseInt(wl_channel_list_5g[i]) > 116 && parseInt(wl_channel_list_5g[i]) < 140){	// belong to 40MHz	
													wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);
												}else if(country == "TW" && parseInt(wl_channel_list_5g[i]) >= 56 && parseInt(wl_channel_list_5g[i]) <= 64){	// belong to 40MHz
													wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);
												}
												else if(country == "JP" && ( parseInt(wl_channel_list_5g[i]) == 132 || parseInt(wl_channel_list_5g[i]) == 136)){		// belong to 40MHz
													wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);													
												}
												else if(document.form.wl_nmode_x.value ==1){
													wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);
												}
												else{
													if((country == "AU" && odmpid == "RT-AC66U_B1") && (parseInt(wl_channel_list_5g[i]) == 116)){
														wl_channel_list_5g[i] = wl_channel_list_5g[i];

													}
													else if((country == "AU" && odmpid == "RT-AC66U_B1") && (parseInt(wl_channel_list_5g[i]) > 116 && parseInt(wl_channel_list_5g[i]) < 140)){
														wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);
													}
													else{
														wl_channel_list_5g[i] = wl_channel_list_5g[i]+"/80";												
													}
												}		
										}
										else{		// for 802.11n, RT-N66U
											wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);							
										}										
								}									
						}
						else if(bw_cap == "3"){	// [80 MHz]
							document.getElementById('wl_nctrlsb_field').style.display = "";
							for(var i=wl_channel_list_5g.length-1;i>=0;i--){																
								if((country == "EU" || country == "E0") && parseInt(wl_channel_list_5g[i]) >= 116 && parseInt(wl_channel_list_5g[i]) <= 140){	// rm 80MHz invalid channel
									wl_channel_list_5g.splice(wl_channel_list_5g.getIndexByValue(wl_channel_list_5g[i]),1);
								}
								else if(country == "TW" && parseInt(wl_channel_list_5g[i]) >= 56 && parseInt(wl_channel_list_5g[i]) <= 64){	// rm 80MHz invalid channel														
									wl_channel_list_5g.splice(wl_channel_list_5g.getIndexByValue(wl_channel_list_5g[i]),1);
								}
								else if(country == "JP" && parseInt(wl_channel_list_5g[i]) >= 132 && parseInt(wl_channel_list_5g[i]) <= 140 ){
									wl_channel_list_5g.splice(wl_channel_list_5g.getIndexByValue(wl_channel_list_5g[i]),1);
								}
								else{
									if((country == "AU" && odmpid == "RT-AC66U_B1") && (parseInt(wl_channel_list_5g[i]) >= 116 && parseInt(wl_channel_list_5g[i]) <= 140)){
										wl_channel_list_5g.splice(wl_channel_list_5g.getIndexByValue(wl_channel_list_5g[i]),1);
									}
									else{
										wl_channel_list_5g[i] = wl_channel_list_5g[i]+"/80";
									}
								}		
							}										
						}
						else if(bw_cap == "2"){		// 40MHz
							document.getElementById('wl_nctrlsb_field').style.display = "";
							if(country == "TW" && wl_channel_list_5g.indexOf("56") != -1){			//remove channel 56, only for country code TW
								var index = wl_channel_list_5g.indexOf("56");
								wl_channel_list_5g.splice(index, 1);
							}

							if((country == "EU" || country == "E0") && wl_channel_list_5g.indexOf("116") != -1){			// remove channel 116, 
								var index = wl_channel_list_5g.indexOf("116");
								wl_channel_list_5g.splice(index, 1);
							}
		
							if((country == "EU" || country == "E0" || country == "JP") && wl_channel_list_5g.indexOf("140") != -1){			// remove channel 140
								index = wl_channel_list_5g.indexOf("140");
								wl_channel_list_5g.splice(index, 1);
							}

							if(country == "AU" && odmpid == "RT-AC66U_B1"){
								if(wl_channel_list_5g.indexOf("116") != -1){
									var index = wl_channel_list_5g.indexOf("116");
									wl_channel_list_5g.splice(index, 1);									
								}

								if(wl_channel_list_5g.indexOf("140") != -1){
									index = wl_channel_list_5g.indexOf("140");
									wl_channel_list_5g.splice(index, 1);
								}
							}
							

							for(var i=0;i<wl_channel_list_5g.length;i++){			
								wl_channel_list_5g[i] = wlextchannel_fourty(wl_channel_list_5g[i]);	
							}		
						}
						else{		//20MHz
							document.getElementById('wl_nctrlsb_field').style.display = "none";
							if(based_modelid == "RT-AC87U" && country == "EU"){		// remove channel between 52 ~ 140 DFS channel of RT-AC87U for EU
								for(i=wl_channel_list_5g.length - 1;i >= 0 ; i--){
									if(parseInt(wl_channel_list_5g[i]) >= 52){
										wl_channel_list_5g.splice(wl_channel_list_5g.getIndexByValue(wl_channel_list_5g[i]),1);
									}
								}
							}
						}
			
						if(based_modelid == "RT-AC87U" && country == "EU"){			//for 5GHz DFS channel of RT-AC87U
							if(bw_cap == "1"){
									document.getElementById('dfs_checkbox').style.display = "none";
									document.form.acs_band1.disabled = true;
							}
							else{
								if(bw_cap != bw_cap_ori){		//switch to Auto
									document.getElementById('dfs_checkbox').style.display = "";
									document.form.acs_band1.disabled = false;
								}
								else{
									if(channel_ori == "0"){
										document.getElementById('dfs_checkbox').style.display = "";
										document.form.acs_band1.disabled = false;
									}
									else{
										document.getElementById('dfs_checkbox').style.display = "none";
										document.form.acs_band1.disabled = true;				
									}
								}			
							}			
						}
						
						if(wl_channel_list_5g[0] != "0")
								wl_channel_list_5g.splice(0,0,"0");
			
						add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, 1);   //construct extension channel
						chanspecs = wl_channel_list_5g;						
				}
				else{	// hook failure to set chennel by static channels array									
						if (bw_cap == "1") { // -- 20 MHz
							if (country == "Q2")
								chanspecs = new Array(0, "36", "40", "44", "48", "149", "153", "157", "161", "165");
							else if (country == "EU")
								chanspecs = new Array(0, "36", "40", "44", "48", "52", "56", "60", "64", "100", "104", "108", "112", "116", "132", "136", "140");
							else if (country == "TW")
								chanspecs = new Array(0, "56", "60", "64", "149", "153", "157", "161", "165");
							else if (country == "CN")
								chanspecs = new Array(0, "149", "153", "157", "161", "165");
							else if (country == "XX")
								chanspecs = new Array(0, "36", "40", "44", "48", "52", "56", "60", "64", "100", "104", "108", "112", "116", "120", "124", "128", "132", "136", "140", "144", "149", "153", "157", "161", "165");
							else // US
								chanspecs = new Array(0, "36", "40", "44", "48", "149", "153", "157", "161", "165");
						}					 
						else if (bw_cap == "2" || (bw_cap == "0" && document.form.preferred_lang.value == "UK")) { // -- [40 MHz] || [20/40 MHz](auto) for UK
							if (country == "Q2")
								chanspecs = new Array(0, "36l", "40u", "44l", "48u", "149l", "153u", "157l", "161u", "165");
							else if (country == "EU")
								chanspecs = new Array(0, "36l", "40u", "44l", "48u", "52l", "56u", "60l", "64u", "100l", "104u", "108l", "112u", "116l", "132l", "136u", "140l");
							else if (country == "TW")
								chanspecs = new Array(0, "60l", "64u", "149l", "153u", "157l", "161u", "165");
							else if (country == "CN")
								chanspecs = new Array(0, "149l", "153u", "157l", "161u", "165");
							else if (country == "XX")
								chanspecs = new Array(0, "36l", "40u", "44l", "48u", "52l", "56u", "60l", "64u", "100l", "104u", "108l", "112u", "116l", "120u", "124l", "128u", "132l", "136u", "140l", "144u", "149l", "153u", "157l", "161u", "165");
							else // US
								chanspecs = new Array(0, "36l", "40u", "44l", "48u", "149l", "153u", "157l", "161u", "165");
						} 
						else if (bw_cap == "3") { // -- 80 MHz
							if (country == "Q2")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "149/80", "153/80", "157/80", "161/80");
							else if (country == "EU")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "52/80", "56/80", "60/80", "64/80", "100/80", "104/80", "108/80", "112/80");
							else if (country == "TW")
								chanspecs = new Array(0, "149/80", "153/80", "157/80", "161/80");
							else if (country == "CN")
								chanspecs = new Array(0, "149/80", "153/80", "157/80", "161/80");
							else if (country == "XX")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "52/80", "56/80", "60/80", "64/80", "100/80", "104/80", "108/80", "112/80", "116/80", "120/80", "124/80", "128/80", "132/80", "136/80", "140/80", "144/80", "149/80", "153/80", "157/80", "161/80");
							else // US
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "149/80", "153/80", "157/80", "161/80");	
						}
						else if (bw_cap == "0" && document.form.preferred_lang.value != "UK") { // -- [20/40/80 MHz] (Auto) for not UK
							if (country == "Q2")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "149/80", "153/80", "157/80", "161/80", "165");
							else if (country == "EU")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "52/80", "56/80", "60/80", "64/80", "100/80", "104/80", "108/80", "112/80", "116l", "132l", "136u", "140l");
							else if (country == "TW")
								chanspecs = new Array(0, "149/80", "153/80", "157/80", "161/80", "165");
							else if (country == "CN")
								chanspecs = new Array(0, "149/80", "153/80", "157/80", "161/80", "165");
							else if (country == "XX")
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "52/80", "56/80", "60/80", "64/80", "100/80", "104/80", "108/80", "112/80", "116/80", "120/80", "124/80", "128/80", "132/80", "136/80", "140/80", "144/80", "149/80", "153/80", "157/80", "161/80", "165");
							else // US
								chanspecs = new Array(0, "36/80", "40/80", "44/80", "48/80", "149/80", "153/80", "157/80", "161/80", "165");
						} 
						else { // ...
								chanspecs = [0];
						}					
				}
		} 
		else if (band == "0") { // - 2.4 GHz				
				if(wl_channel_list_2g instanceof Array && wl_channel_list_2g != ["0"]){	//With wireless channel 2.4g hook or return ["0"]
						wl_channel_list_2g = eval('<% channel_list_2g(); %>');
						if(wl_channel_list_2g[0] != "0")
								wl_channel_list_2g.splice(0,0,"0");
					
						if(cur.search('[ul]') != -1){
							cur_extend_channel = cur.slice(-1);			//current control channel
							cur_control_channel = cur.split(cur_extend_channel)[0];	//current extension channel direction
						}
						else{
							cur_control_channel = cur;
						}				
						
						if(bw_cap == "2" || bw_cap == "0") { 	// -- [40 MHz]  | [20/40 MHz]				
							document.getElementById('wl_nctrlsb_field').style.display = "";
							if(cur_control_channel == 0){
								extend_channel = ["<#Auto#>"];
								extend_channel_value = ["1"];
								add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, 1);	
							}
							else if(cur_control_channel >= 1 && cur_control_channel <= 4){
								extend_channel = ["<#WLANConfig11b_EChannelAbove#>"];
								add_options_x2(document.form.wl_nctrlsb, extend_channel, "l");							
							}
							else if(wl_channel_list_2g.length == 12){    // 1 ~ 11
								if(cur_control_channel >= 5 && cur_control_channel <= 7){
									extend_channel = ["<#WLANConfig11b_EChannelAbove#>", "<#WLANConfig11b_EChannelBelow#>"];
									extend_channel_value = ["l", "u"];
									add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, cur_extend_channel);							
								}
								else if(cur_control_channel >= 8 && cur_control_channel <= 11){
									extend_channel = ["<#WLANConfig11b_EChannelBelow#>"];
									extend_channel_value = ["u"];
									add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, cur_extend_channel);								
								}
							}
							else{		// 1 ~ 13
								if(cur_control_channel >= 5 && cur_control_channel <= 9){
									extend_channel = ["<#WLANConfig11b_EChannelAbove#>", "<#WLANConfig11b_EChannelBelow#>"];
									extend_channel_value = ["l", "u"];
									add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, cur_extend_channel);							
								}
								else if(cur_control_channel >= 10 && cur_control_channel <= 13){
									extend_channel = ["<#WLANConfig11b_EChannelBelow#>"];
									extend_channel_value = ["u"];
									add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, cur_extend_channel);								
								}			
							}
						}else{		// -- [20 MHz]	
							cur_control_channel = cur;
							document.getElementById('wl_nctrlsb_field').style.display = "none";
						}	
						
						chanspecs = wl_channel_list_2g;
				}else{			
						if (bw_cap == "1") { // -- 20 MHz
							if (country == "Q2")
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11");
							else if (country == "EU")
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13");
							else if (country == "TW")
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11");
							else if (country == "CN")
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13");
							else if (country == "XX")
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14");
							else // US
								chanspecs = new Array(0, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11");
						} 
						else if (bw_cap == "2" || bw_cap == "0") { // -- 40 MHz
							if (country == "Q2")
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8u", "9u", "10u", "11u");
							else if (country == "EU")
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8l", "8u", "9l", "9u", "10u", "11u", "12u", "13u");
							else if (country == "TW")
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8u", "9u", "10u", "11u");
							else if (country == "CN")
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8l", "8u", "9l", "9u", "10u", "11u", "12u", "13u");
							else if (country == "XX")
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8l", "8u", "9l", "9u", "10u", "11u", "12u", "13u", "14u");
							else // US
								chanspecs = new Array(0, "1l", "2l", "3l", "4l", "5l", "5u", "6l", "6u", "7l", "7u", "8u", "9u", "10u", "11u");
						}
						else { // ...
							chanspecs = [0];
						}
				}		
		}
		else if(band == "2"){	// 5GHz - high band
						if(based_modelid == "RT-AC3200"){
							if(country == "E0")
								wl_channel_list_5g_2 = new Array("100","104","108","112","116","132","136","140");
							else if(country == "JP")
								wl_channel_list_5g_2 = new Array("100","104","108","112","116","120","124","128","132","136","140");
							else	//US
								wl_channel_list_5g_2 = new Array("149","153","157","161","165");
						}
						else
							wl_channel_list_5g_2 = eval('<% channel_list_5g_2(); %>');
					
						extend_channel = ["<#Auto#>"];		 // for 5GHz, extension channel always displays Auto
						extend_channel_value = [""];				
						if (bw_cap != "0" && bw_cap != "1" && wl_channel_list_5g_2.getIndexByValue("165") >= 0 ) // rm 165, If not [20 MHz] or not [Auto]
								wl_channel_list_5g_2.splice(wl_channel_list_5g_2.getIndexByValue("165"),1);

						if(country == "E0"){	//weather radar channels
							if(wl_channel_list_5g_2.getIndexByValue("120") >= 0)
								wl_channel_list_5g_2.splice(wl_channel_list_5g_2.getIndexByValue("120"),1);
							if(wl_channel_list_5g_2.getIndexByValue("124") >= 0)
								wl_channel_list_5g_2.splice(wl_channel_list_5g_2.getIndexByValue("124"),1);
							if(wl_channel_list_5g_2.getIndexByValue("128") >= 0)
								wl_channel_list_5g_2.splice(wl_channel_list_5g_2.getIndexByValue("128"),1);
						}
						
						if(bw_cap == "0"){	// [20/40/80 MHz] (auto)
							document.getElementById('wl_nctrlsb_field').style.display = "";
							for(var i=0;i<wl_channel_list_5g_2.length;i++){
								if(wl_channel_list_5g_2[i] == "165" || wl_channel_list_5g_2[i] == "140")		//165 belong to 20MHz
									wl_channel_list_5g_2[i] = wl_channel_list_5g_2[i];
								else if((wl_channel_list_5g_2[i] == "56") && country == "TW")		//56 belong 20MHz only for TW
									wl_channel_list_5g_2[i] = wl_channel_list_5g_2[i];
								else if(band5g_11ac_support){
									if(country == "EU" && parseInt(wl_channel_list_5g_2[i]) == 116){		//	belong to 20MHz
									wl_channel_list_5g_2[i] = wl_channel_list_5g_2[i];
									}else if((country == "EU" || country == "E0")&& parseInt(wl_channel_list_5g_2[i]) > 116 && parseInt(wl_channel_list_5g_2[i]) < 140){	// belong to 40MHz												
										wl_channel_list_5g_2[i] = wlextchannel_fourty(wl_channel_list_5g_2[i]);
									}else if(country == "TW" && parseInt(wl_channel_list_5g_2[i]) >= 56 && parseInt(wl_channel_list_5g_2[i]) <= 64){	// belong to 40MHz
										wl_channel_list_5g_2[i] = wlextchannel_fourty(wl_channel_list_5g_2[i]);
									}else if(country == "JP" && ( parseInt(wl_channel_list_5g_2[i]) == 132 || parseInt(wl_channel_list_5g_2[i]) == 136)){		// belong to 40MHz
										wl_channel_list_5g_2[i] = wlextchannel_fourty(wl_channel_list_5g_2[i]);														
									}else if(document.form.wl_nmode_x.value ==1){
										wl_channel_list_5g_2[i] = wlextchannel_fourty(wl_channel_list_5g_2[i]);
									}else{	
										wl_channel_list_5g_2[i] = wl_channel_list_5g_2[i]+"/80";												
									}		
								}
								else{		// for 802.11n, RT-N66U
									wl_channel_list_5g_2[i] = wlextchannel_fourty(wl_channel_list_5g_2[i]);								
								}										
							}								
						}
						else if(bw_cap == "3"){	// [80 MHz]
							document.getElementById('wl_nctrlsb_field').style.display = "";
								for(var i=wl_channel_list_5g_2.length-1;i>=0;i--){									

												if((country == "EU" || country == "E0") && parseInt(wl_channel_list_5g_2[i]) >= 116 && parseInt(wl_channel_list_5g_2[i]) <= 140){	// rm 80MHz invalid channel
														wl_channel_list_5g_2.splice(wl_channel_list_5g_2.getIndexByValue(wl_channel_list_5g_2[i]),1);
												}else if(country == "TW" && parseInt(wl_channel_list_5g_2[i]) >= 56 && parseInt(wl_channel_list_5g_2[i]) <= 64){	// rm 80MHz invalid channel														
														wl_channel_list_5g_2.splice(wl_channel_list_5g_2.getIndexByValue(wl_channel_list_5g_2[i]),1);
												}else if(based_modelid == "RT-AC3200" && country == "JP" && parseInt(wl_channel_list_5g_2[i]) >= 132 && parseInt(wl_channel_list_5g_2[i]) <= 140){	// rm 80MHz invalid channel
														wl_channel_list_5g_2.splice(wl_channel_list_5g_2.getIndexByValue(wl_channel_list_5g_2[i]),1);
												}else{																																																										
														wl_channel_list_5g_2[i] = wl_channel_list_5g_2[i]+"/80";
												}	
								}										
						}
						else if(bw_cap == "2"){		// 40MHz
							document.getElementById('wl_nctrlsb_field').style.display = "";
								if(country == "TW" && wl_channel_list_5g_2.indexOf("56") != -1){			//remove channel 56, only for country code TW
									var index = wl_channel_list_5g_2.indexOf("56");
									wl_channel_list_5g_2.splice(index, 1);
								}

								if(wl_channel_list_5g_2.indexOf("116") != -1 && !(based_modelid == "RT-AC3200" && country == "JP")){			// remove channel 116, 
									var index = wl_channel_list_5g_2.indexOf("116");
									wl_channel_list_5g_2.splice(index, 1);
								}
		
								if(wl_channel_list_5g_2.indexOf("140") != -1){			// remove channel 140
									index = wl_channel_list_5g_2.indexOf("140");
									wl_channel_list_5g_2.splice(index, 1);
								}

								for(var i=0;i<wl_channel_list_5g_2.length;i++){			
									wl_channel_list_5g_2[i] = wlextchannel_fourty(wl_channel_list_5g_2[i]);	
								}
						}
						else{		//20MHz
							document.getElementById('wl_nctrlsb_field').style.display = "none";					
						}
			
						if(wl_channel_list_5g_2[0] != "0")
								wl_channel_list_5g_2.splice(0,0,"0");
			
						add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, 1);   //construct extension channel
						chanspecs = wl_channel_list_5g_2;				
		}	//end 5GHz - high band
	} 
	else { // b/g mode 
		chanspecs = new Array(0);
	}

	/* Reconstruct channel array from new chanspecs */
	document.form.wl_channel.length = chanspecs.length;
	if(band == 1 || band == 2){
		for (var i in chanspecs){
			if (chanspecs[i] == 0)
				document.form.wl_channel[i] = new Option("<#Auto#>", chanspecs[i]);
			else
				document.form.wl_channel[i] = new Option(chanspecs[i].toString().replace("/80", "").replace("u", "").replace("l", ""), chanspecs[i]);
			document.form.wl_channel[i].value = chanspecs[i];
			if (chanspecs[i] == cur && bw_cap == '<% nvram_get("wl_bw"); %>'){
				document.form.wl_channel[i].selected = true;
				sel = 1;
			}
		}
		if (sel == 0 && document.form.wl_channel.length > 0)
			document.form.wl_channel[0].selected = true;
	}
	else{
		for(i=0;i< chanspecs.length;i++){
			if(i == 0)
				chanspecs_string[i] = "<#Auto#>";
			else
				chanspecs_string[i] = chanspecs[i];
			
		}

		add_options_x2(document.form.wl_channel, chanspecs_string, chanspecs, cur_control_channel);
	}
}

function wlextchannel_fourty(v){
	var tmp_wl_channel_5g = "";
	if(v > 144){
		tmp_wl_channel_5g = v - 1;
		if(tmp_wl_channel_5g%8 == 0)
			v = v + "u";
		else		
			v = v + "l";
	}else{
		if(v%8 == 0)
			v = v + "u";
		else		
			v = v + "l";											
	}
	
	return v;
}

var wl1_dfs = '<% nvram_get("wl1_dfs"); %>';
function change_channel(obj){
	var extend_channel = new Array();
	var extend_channel_value = new Array();
	var selected_channel = obj.value;
	var channel_length =obj.length;
	var band = document.form.wl_unit.value;
	if(document.form.wl_bw.value != 1){   // 20/40 MHz or 40MHz
		if(channel_length == 12){    // 1 ~ 11
			if(selected_channel >= 1 && selected_channel <= 4){
				extend_channel = ["<#WLANConfig11b_EChannelAbove#>"];
				extend_channel_value = ["l"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);				
			}
			else if(selected_channel >= 5 && selected_channel <= 7){
				extend_channel = ["<#WLANConfig11b_EChannelAbove#>", "<#WLANConfig11b_EChannelBelow#>"];
				extend_channel_value = ["l", "u"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);							
			}
			else if(selected_channel >= 8 && selected_channel <= 11){
				extend_channel = ["<#WLANConfig11b_EChannelBelow#>"];
				extend_channel_value = ["u"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);								
			}
			else{				//for 0: Auto
				extend_channel = ["<#Auto#>"];
				extend_channel_value = [""];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);
			}
		}
		else{		// 1 ~ 13
			if(selected_channel >= 1 && selected_channel <= 4){
				extend_channel = ["<#WLANConfig11b_EChannelAbove#>"];
				extend_channel_value = ["l"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);							
			}
			else if(selected_channel >= 5 && selected_channel <= 9){
				extend_channel = ["<#WLANConfig11b_EChannelAbove#>", "<#WLANConfig11b_EChannelBelow#>"];
				extend_channel_value = ["l", "u"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);							
			}
			else if(selected_channel >= 10 && selected_channel <= 13){
				extend_channel = ["<#WLANConfig11b_EChannelBelow#>"];
				extend_channel_value = ["u"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);								
			}
			else{				//for 0: Auto
				extend_channel = ["<#Auto#>"];
				extend_channel_value = [""];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);
			}
		}
	}
	
	if(band == 1){
		if(country == "EU"){		// for DFS channel
			if(based_modelid == "RT-AC68U" || based_modelid == "RT-AC68A" || based_modelid == "4G-AC68U" || based_modelid == "DSL-AC68U"
			|| (based_modelid == "RT-AC66U" && wl1_dfs == "1")
			|| based_modelid == "RT-N66U"){
				if(document.form.wl_channel.value  == 0){
					document.getElementById('dfs_checkbox').style.display = "";
					document.form.acs_dfs.disabled = false;
				}	
				else{
					document.getElementById('dfs_checkbox').style.display = "none";
					document.form.acs_dfs.disabled = true;
				}
			}
			else if(based_modelid == "RT-AC87U"){
				if(document.form.wl_channel.value  == "0"){
					if(document.form.wl_bw.value == "1"){
						document.getElementById('dfs_checkbox').style.display = "none";
						document.form.acs_dfs.disabled = true;
					}
					else{
						document.getElementById('dfs_checkbox').style.display = "";
						document.form.acs_dfs.disabled = false;
					}
				}
				else{
					document.getElementById('dfs_checkbox').style.display = "none";
					document.form.acs_dfs.disabled = true;
				}				
			}
		}
		else if(country == "US" || country == "SG"){			//for acs band1 channel
			if(based_modelid == "RT-AC68U" || based_modelid == "RT-AC68A" || based_modelid == "4G-AC68U" || based_modelid == "DSL-AC68U"
			|| based_modelid == "RT-AC56U" || based_modelid == "RT-AC56S"
			|| based_modelid == "RT-AC66U"
			|| based_modelid == "RT-N66U"
			|| based_modelid == "RT-AC53U"){
				if(document.form.wl_channel.value  == 0){
					document.getElementById('acs_band1_checkbox').style.display = "";
					document.form.acs_band1.disabled = false;
				}	
				else{
					document.getElementById('acs_band1_checkbox').style.display = "none";
					document.form.acs_band1.disabled = true;
				}
			}
		}
	}
	else if(band == 0){
		if(country == "EU" || country == "JP" || country == "SG" || country == "CN" || country == "UA" || country == "KR"){
			if(!Qcawifi_support && !Rawifi_support){
				if(document.form.wl_channel.value  == '0'){
					document.getElementById('acs_ch13_checkbox').style.display = "";
					document.form.acs_ch13.disabled = false;					
				}
				else{
					document.getElementById('acs_ch13_checkbox').style.display = "none";
					document.form.acs_ch13.disabled = true;							
				}
			}
		}
	}	
}
