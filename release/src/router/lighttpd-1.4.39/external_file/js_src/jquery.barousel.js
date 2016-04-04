/*=============================    
jquery.barousel.js 
v.0.1
Julien Decaudin - 03/2010
www.juliendecaudin.com
=============================*/

(function ($) {
	
	$.fn.next = function(settings){
		if (settings && settings.navFreeze == 0)			
			next(settings);
	};
	
	$.fn.prev = function(settings){
		if (settings && settings.navFreeze == 0)			
			prev(settings);
	};
	
	$.fn.adjustLayout = function(settings){
		if(settings){
			var currentImage = settings.imageList[settings.currentIndex];
			fitImage(settings, currentImage, settings.currentIndex);
		}
	};
	
	$.fn.close = function(settings){
		if(settings){
			close(settings);
		}
	};
	
    $.fn.barousel = function (callerSettings) {
    	
        var settings = $.extend({
        	name: '',
            imageWrapper: '.barousel_image',
            contentWrapper: '.barousel_content',
            contentLinksWrapper: null,
            navWrapper: '.barousel_nav',
            slideDuration: 3000, //duration of each slide in milliseconds
            fadeIn: 1, //fade between slides; activated by default
            fadeInSpeed: 500, //fade duration in milliseconds 
            manualCarousel: 0, //manual carousel if set to 1
            contentResize: 1, //resize content container
            contentResizeSpeed: 300, //content resize speed in milliseconds
            debug: 0,
            startIndex: 0,
            hideNavBarDuration: 3000,
            onLoading: 0,
            showExifMode: 0,
            storage: null,
            stringTable: null,
            exifData: null,
            contextMenu: null,
            closeHandler: null,
            initCompleteHandler: null, 
            enableExifFunc: 0,
            enableShareFunc: 0
        }, callerSettings || {});

        settings.imageWrapper = $(this).find(settings.imageWrapper);
        settings.contentWrapper = $(this).find(settings.contentWrapper);
        settings.contentLinksWrapper = $(settings.contentLinksWrapper);
        settings.navWrapper = $(this).find(settings.navWrapper);
        settings.imageList = settings.imageWrapper.find('img').not('[class*=intro]'); //list of the items' background image (intro image is ignored)
        settings.contentList = settings.contentWrapper.find('div').not('[class*=intro]'); //list of the items' content (intro content is ignored)
        settings.contentLinksList = settings.contentLinksWrapper.find('a'); //list of the items' content links (optional)
        settings.imageIntro = settings.imageWrapper.find('img[class*=intro]');
        settings.contentIntro = settings.contentWrapper.find('div[class*=intro]');
        settings.currentIndex = 0; //index of the current displayed item
        settings.totalItem = settings.imageList.length; //total number of items
        settings.stopCarousel = 0; //flag to stop the carousel
        settings.timerCarousel = 0; //timer for the carousel
        settings.navFreeze = 0; //flag to avoid clicking too fast on the nav
        settings.introActive = 0; //flag to know if there is an intro state and if it's active
		settings.timerHideNavBar = 0;
		settings.imageSize = new Array(settings.totalItem);
		settings.prevImage = null;
		
        if (settings.imageWrapper.find('img[class*=intro]').length > 0) {
            settings.introActive = 1;
        }
				
		//set the index of each image  
        settings.imageList.each(function (n) {
        	
        	var end_load = function(settings, n){
        		currentImage = $(settings.imageList[n]);
        		var src = currentImage.attr("src");
        		var class_name = currentImage.attr('class');
        		
        		if(class_name=="current"){
        			
        			settings.currentIndex = n;
        			
        			$(settings.contentList).hide();
        			
        			loadModuleContent(settings);
        			
	        		if(settings.manualCarousel==0){	
	        			if (settings.stopCarousel == 0) {
							var nextIndex;
							if (settings.currentIndex == settings.totalItem - 1) {
								nextIndex = 0;
							} else {
								nextIndex = parseInt(settings.currentIndex) + 1;
							}
							   	
							if (settings.manualCarousel == 0){
								settings.timerCarousel = window.setTimeout(function(){
									loadItem(settings, nextIndex);
								}, settings.slideDuration);
							}
						}
	        		}
	        		
					settings.onLoading = 0;
					settings.navFreeze = 0;
					showLoading(false);
				}
        	};
        	
        	this.onload = function(e){
        		
        		settings.currentIndex = n;
        		
				settings.imageSize[settings.currentIndex] = { width:$(this).width(),
															  height:$(this).height() };
        		
        		currentImage = $(settings.imageList[settings.currentIndex]);
				
				if(settings.enableExifFunc==1){
					currentImage.exifLoad(function(exif_data){
						
						settings.exifData = (!exif_data) ? null : exif_data;
						
		        		fitImage(settings, currentImage.get(0), settings.currentIndex); 
		        			
		        		currentImage.fadeIn(settings.fadeInSpeed, function () {
			          		
							if(settings.prevImage){
								settings.prevImage.hide();	          	 
								settings.prevImage.removeClass('previous');
							}
							
					       	end_load(settings, n);
					       	
					       	if(settings.showExifMode==1)
					       		showExifInfo(settings, currentImage, true);
				       	});
			       	});
		    	}
		    	else{
		    		settings.exifData = null;
		    		
		    		fitImage(settings, currentImage.get(0), settings.currentIndex);
		    		
		    		currentImage.fadeIn(settings.fadeInSpeed, function () {
			         	
						if(settings.prevImage){
							settings.prevImage.hide();	          	 
							settings.prevImage.removeClass('previous');
						}
								
						end_load(settings, n);
					});
		    	}
        	};
        	
        	this.onerror = function(e){
        		end_load(settings, n);
        	};
        	
         	this.index = n; 
        });
						
        //set the index of each content  
        settings.contentList.each(function (n) { this.index = n; });

        //set the index of each content link (optional) 
        settings.contentLinksList.each(function (n) { this.index = n; });
		
        //return error if different number of images and contents
        if (settings.imageList.length != settings.contentList.length) {
        	/* DEBUG */
            if (settings.debug == 1) trace('[Barousel error] images and contents must be the same number');
            return this;
        }

        //init the default content height
        if (settings.contentResize == 1 && settings.introActive == 0) {
        	$(settings.contentWrapper).height($(settings.contentList[settings.currentIndex]).height() + 10);
        }

        //init the content link default state (optional)
        if (settings.contentLinksWrapper != null) {
        	$(settings.contentLinksList[settings.currentIndex]).addClass('current');
        }

        //build the navigation
		var strNavList = "";
        strNavList += "<div class='counter'><span class='counter_current'>1</span>/<span class='counter_total'>" + settings.totalItem + "</span></div>";
               
        if (settings.totalItem > 1){
        	strNavList += "<div class='nav_btn nav_btn_icon prev transparent'><a href='#' title='prev'>&nbsp;</a></div>";
            strNavList += "<div class='nav_btn nav_btn_icon next transparent'><a href='#' title='next'>&nbsp;</a></div>";
            strNavList += "<div class='nav_btn_icon play transparent'><a href='#' title='play'>&nbsp;</a></div>";
            strNavList += "<div class='nav_btn_icon pause transparent'><a href='#' title='pause'>&nbsp;</a></div>";
		}
                
        strNavList += "<div class='nav_btn nav_btn_icon close transparent'><a href='#' title='close'>&nbsp;</a></div>";
        
        if(settings.enableExifFunc==1 && !(isIE()&&getInternetExplorerVersion()<=10)){
	        strNavList += '<div class="image_exif_wrapper turn_c">';
	        strNavList += '<table><tr>';
	        strNavList += '<td><span>' + settings.stringTable.getString("title_exif_info") + ':</span></td>';
	        strNavList += '<td></td>';
	        strNavList += '<td><span>' + settings.stringTable.getString("title_exif_off") + '</span></td>';
			strNavList += '<td><div class="image_exif turn_m">';
			
			if(settings.storage){
				settings.showExifMode = settings.storage.getl("showExifMode") == undefined ? 0 : settings.storage.getl("showExifMode");
				strNavList += '<a href="#"><span class="icon" style="left: ' + ((settings.showExifMode==1) ? '25px;' : '0px') + '"></span></a>';
			}
			
			strNavList += '</div></td>';
			strNavList += '<td><span>' + settings.stringTable.getString("title_exif_on") + '</span></td>';
			strNavList += '</tr></table>';
			strNavList += '</div>';
		}
		
		if(settings.enableShareFunc==1)
			strNavList += '<div class="albutton toolbar-button-right" id="btnShareImage" style="display:block"><div class="ticon"></div></div>';
		
        settings.navWrapper.append(strNavList);
        settings.navList = settings.navWrapper.find('a'); //list of the items' nav link
		        
        $(".play").css("display", "block");
        $(".pause").css("display", "none");
        
        if(settings.enableShareFunc==1){
        	
        	$("#btnShareImage").attr("title", m.getString('btn_sharelink'));
        
	        settings.contextMenu = $.contextMenu({
	        	selector: 'div#btnShareImage', 
				trigger: 'left',
				callback: function(key, options) {
					if(key=="upload2facebook"){
						var currentImage = $(settings.imageList[settings.currentIndex]);
						var upload_files = new Array(0);
						upload_files.push(currentImage.attr("uhref"));
						open_upload2service_window("facebook", upload_files);
						upload_files = null;
					}
					else if(key=="upload2flickr"){
						var currentImage = $(settings.imageList[settings.currentIndex]);
						var upload_files = new Array(0);
						upload_files.push(currentImage.attr("uhref"));
						open_upload2service_window("flickr", upload_files);
						upload_files = null;
					}
					else if(key=="upload2picasa"){
						var currentImage = $(settings.imageList[settings.currentIndex]);
						var upload_files = new Array(0);
						upload_files.push(currentImage.attr("uhref"));
						open_upload2service_window("picasa", upload_files);
						upload_files = null;
					}
					else if(key=="upload2twitter"){
						var currentImage = $(settings.imageList[settings.currentIndex]);
						var upload_files = new Array(0);
						upload_files.push(currentImage.attr("uhref"));
						open_upload2service_window("twitter", upload_files);
						upload_files = null;
					}
					else if(key=="share2facebook"){
						var currentImage = $(settings.imageList[settings.currentIndex]);
						var selectFileArray = new Array(0);						
						selectFileArray.push(settings.storage.get('openurl') + "/" + currentImage.attr("file"));
						open_sharelink_window("facebook", selectFileArray);
						selectFileArray = null;
					}
					else if(key=="share2googleplus"){
						var currentImage = $(settings.imageList[settings.currentIndex]);
						var selectFileArray = new Array(0);
						selectFileArray.push(settings.storage.get('openurl') + "/" + currentImage.attr("file"));
						open_sharelink_window("googleplus", selectFileArray);
						selectFileArray = null;
					}
					else if(key=="share2twitter"){
						var currentImage = $(settings.imageList[settings.currentIndex]);
						var selectFileArray = new Array(0);
						selectFileArray.push(settings.storage.get('openurl') + "/" + currentImage.attr("file"));
						open_sharelink_window("twitter", selectFileArray);
						selectFileArray = null;
					}
					else if(key=="share2plurk"){
						var currentImage = $(settings.imageList[settings.currentIndex]);
						var selectFileArray = new Array(0);
						selectFileArray.push(settings.storage.get('openurl') + "/" + currentImage.attr("file"));
						open_sharelink_window("plurk", selectFileArray);
						selectFileArray = null;
					}
					else if(key=="share2weibo"){
						var currentImage = $(settings.imageList[settings.currentIndex]);
						var selectFileArray = new Array(0);
						selectFileArray.push(settings.storage.get('openurl') + "/" + currentImage.attr("file"));
						open_sharelink_window("weibo", selectFileArray);
						selectFileArray = null;
					}
					else if(key=="share2qq"){
						var currentImage = $(settings.imageList[settings.currentIndex]);
						var selectFileArray = new Array(0);
						selectFileArray.push(settings.storage.get('openurl') + "/" + currentImage.attr("file"));
						open_sharelink_window("qq", selectFileArray);
						selectFileArray = null;
					}
		        },
		        items: {
					"submenu_upload": { 
						name : settings.stringTable.getString("title_upload2"),
		                items : {
		                	"upload2facebook": {
								name: settings.stringTable.getString("title_facebook")
							},
							"upload2flickr": {
								name: settings.stringTable.getString("title_flickr")
							},
							"upload2picasa": {
								name: settings.stringTable.getString("title_picasa")
							},
							"upload2twitter": {
								name: settings.stringTable.getString("title_twitter")
							}
		                }
		            },
					"submenu_share": { 
						name : settings.stringTable.getString("title_share2"),
		                items : {
		                	"share2facebook": {
								name: settings.stringTable.getString("title_facebook")
							},
							"share2googleplus": {
								name: settings.stringTable.getString("title_googleplus")
							},
							"share2twitter": {
								name: settings.stringTable.getString("title_twitter")
							},
							"share2plurk": {
								name: settings.stringTable.getString("title_plurk")
							},
							"share2weibo": {
								name: settings.stringTable.getString("title_weibo")
							},
							"share2qq": {
								name: settings.stringTable.getString("title_qq")
							}
		                }
		            }
		        }
	    	});
    	}
    	
		//prev/next navigation type
        settings.navList.each(function () {
        	$(this).click(function () {
            	if (settings.navFreeze == 0) {
                	window.clearTimeout(settings.timerCarousel);
                    settings.stopCarousel = 1;

                    if ($(this).parent().hasClass('prev')) {
                    	prev(settings);
                    } 
                    else if ($(this).parent().hasClass('next')) {
                    	next(settings);
                    } 
                    else if ($(this).parent().hasClass('close')) {
                    	close(settings);
                    } 
                    else if ($(this).parent().hasClass('play')) {
                    	settings.manualCarousel = 0;
                        settings.stopCarousel = 0;
                            	
                        $(".play").css("display", "none");
                		$(".pause").css("display", "block");
                
                        var nextIndex;
											
						if (settings.currentIndex == settings.totalItem - 1) {
							nextIndex = 0;
						} else {
						   	nextIndex = parseInt(settings.currentIndex) + 1;
						}
											        
						if (settings.manualCarousel == 0){
							var loadItemCall = function () { loadItem(settings, nextIndex); };
							settings.timerCarousel = window.setTimeout(loadItemCall, settings.slideDuration);
						}
                    }
                    else if ($(this).parent().hasClass('pause')) {
                    	settings.manualCarousel = 1;
                        settings.stopCarousel = 1;
                            	
                        $(".play").css("display", "block");
                		$(".pause").css("display", "none");
                    }
                    else if ($(this).parent().hasClass('image_exif')) {
                    	if(settings.showExifMode==0){
                    		$(this).parent().find('a span.icon').css('left', '25px');
							settings.showExifMode = 1;
							
							var currentImage = $(settings.imageList[settings.currentIndex]);
							showExifInfo(settings, currentImage, true);
							
							if(settings.storage)
								settings.storage.setl("showExifMode", 1);
						}
						else{
							$(this).parent().find('a span.icon').css('left', '0px');
							settings.showExifMode = 0;
							
							var currentImage = $(settings.imageList[settings.currentIndex]);
							showExifInfo(settings, currentImage, false);
							
							if(settings.storage)
								settings.storage.setl("showExifMode", 0);
						}
                  	}
				}            
                
                settings.introActive = 0;
                return false;
			});
		});
                
		settings.navWrapper.mousedown(function () {
        	showHideNavBar(settings, true);
        });
                
        settings.navWrapper.mousemove(function () {                
        	showHideNavBar(settings, true);
        });
               	
        settings.imageWrapper.mousemove(function () {                
        	showHideNavBar(settings, true);
        });
                
        var showHideNavBarCall = function(){ showHideNavBar(settings, false); };
        settings.timerHideNavBar = window.setInterval(showHideNavBarCall, settings.hideNavBarDuration);
        					
        //start the carousel
        if (settings.manualCarousel == 0) {
           	var loadItemCall = function () { loadItem(settings, 1); };
           	settings.timerCarousel = window.setTimeout(loadItemCall, settings.slideDuration);
        }
        else{
           	loadItem(settings, settings.startIndex, 1);
        }
       	
       	if(settings.initCompleteHandler)
       		settings.initCompleteHandler(settings);
       	
        return this;
    };
	
	var prev = function(settings){
		settings.manualCarousel = 1;
        settings.stopCarousel = 1;
                           	
        $(".play").css("display", "block");
        $(".pause").css("display", "none");
                			
		var previousIndex;
											
        if (parseInt(settings.currentIndex) == 0) {
         	previousIndex = parseInt(settings.totalItem) - 1;
        } else {
          	previousIndex = parseInt(settings.currentIndex) - 1;
        }
        loadItem(settings, previousIndex);
        settings.currentIndex = previousIndex;
	};
	
	var next = function(settings){
		settings.manualCarousel = 1;
        settings.stopCarousel = 1;
                           	
        $(".play").css("display", "block");
        $(".pause").css("display", "none");
        
		var nextIndex;
																
        if (parseInt(settings.currentIndex) == (parseInt(settings.totalItem) - 1)) {
        	nextIndex = 0;
        } else {
          	nextIndex = parseInt(settings.currentIndex) + 1;
        }
        
        loadItem(settings, nextIndex);
        settings.currentIndex = nextIndex;
	};
	
	var close = function(settings){
		if(settings){
			settings.manualCarousel = 1;
            settings.stopCarousel = 1;
                        
			window.clearTimeout(settings.timerCarousel);
            
            if(settings.enableShareFunc==1)	
            	$.contextMenu( 'destroy', 'div#btnShareImage' );
			
			$("#"+settings.name).remove();
			
            if(settings.closeHandler){
            	settings.closeHandler();
            }
    	}
	};
	
	var showHideNavBar = function(settings, bshow){
		if(bshow){
			$(settings.navWrapper).show();
				
			window.clearInterval(settings.timerHideNavBar);
			var showHideNavBarCall = function(){ showHideNavBar(settings, false); };
        	settings.timerHideNavBar = window.setInterval(showHideNavBarCall, settings.hideNavBarDuration);
		}
		else{
			$(settings.navWrapper).hide();
		}
	};
	
	var radians = function(angle) {
		if (typeof angle == 'number') return angle;
		return {
			rad: function(z) {
				return z;
			},
			deg: function(z) {
				return Math.PI / 180 * z;
			}
		}[String(angle).match(/[a-z]+$/)[0] || 'rad'](parseFloat(angle));
	};
	
	var fitImage = function(settings, ImgD, index) {
		var exif_data = settings.exifData;
		var window_width = $(window).width();
		var window_height = $(window).height();
		var orientation = "0";
		var imgWidth = settings.imageSize[index].width;
		var imgHeight = settings.imageSize[index].height;
		var currentImage = $(settings.imageList[index]);
		
		if(imgWidth<=0 || imgHeight<=0)
			return;
		
		if(isIE()&&getInternetExplorerVersion()<=8){
			var newImgWidth = imgWidth;
			var newImgHeight = imgHeight;
			if( newImgWidth > window_width ){
				newImgWidth = window_width;
				newImgHeight = (imgHeight*newImgWidth)/imgWidth;
			}
					
			if( newImgHeight > window_height ){
				newImgHeight = window_height;
				newImgWidth = (imgWidth*newImgHeight)/imgHeight;
			}
			
			ImgD.width = newImgWidth;
			ImgD.height = newImgHeight;
			ImgD.style.left = (window_width-newImgWidth)/2 +"px";
			ImgD.style.top = (window_height-newImgHeight)/2 +"px";
			
			return;
		}
		
		ImgD.width = imgWidth;
		ImgD.height = imgHeight;
		ImgD.style.left = (window_width-imgWidth)/2 +"px";
		ImgD.style.top = (window_height-imgHeight)/2 +"px";
		
		if(exif_data){
			var ori = exif_data["Orientation"];
			
			if(ori==8) orientation = "270deg";
			else if(ori==3) orientation = "180deg";
			else if(ori==6) orientation = "90deg";
			
			if(orientation!="0"){
				var angle = radians(orientation);
				var sin = Math.sin(angle);
				var cos = Math.cos(angle);
				var rotateWidth = Math.floor(Math.abs(sin) * imgHeight + Math.abs(cos) * imgWidth);
				var rotateHeight = Math.floor(Math.abs(cos) * imgHeight + Math.abs(sin) * imgWidth);
				
				imgWidth = rotateWidth;
				imgHeight = rotateHeight;
			}
		}
			
		var newImgWidth = imgWidth;
		var newImgHeight = imgHeight;
		if( newImgWidth > window_width ){
			newImgWidth = window_width;
			newImgHeight = (imgHeight*newImgWidth)/imgWidth;
		}
				
		if( newImgHeight > window_height ){
			newImgHeight = window_height;
			newImgWidth = (imgWidth*newImgHeight)/imgHeight;
		}
				
		var transform = "scale("+(newImgWidth/imgWidth) +", "+(newImgHeight/imgHeight)+")";
		
		if(orientation!="0"){
			transform += " rotate("+orientation+")";
		}
		/*
		if(isIE()&&getInternetExplorerVersion()<=8){
			var filter = "progid:DXImageTransform.Microsoft.Matrix(sizingMethod='auto expand',M11="+(newImgWidth/imgWidth)+",M12=0,M21=0,M22="+(newImgWidth/imgWidth)+")";
			ImgD.style.filter = filter;
			
			currentImage.css("-ms-filter", filter);
			currentImage.css("margin-left", "-500px");
	   		currentImage.css("margin-top", "-500px");
   		}
   		else*/{
			currentImage.css("-ms-transform", transform);  //- FF3.5/3.6
			currentImage.css("-moz-transform", transform);  //- FF3.5/3.6
			currentImage.css("-o-transform", transform);  //- Opera 10.5
			currentImage.css("-webkit-transform", transform); //- Saf3.1+
			currentImage.css("transform", transform); //- Newer browsers (incl IE9)
		}
	};
								
    var loadItem = function (settings, index, bStartImage) {
    		
		showLoading(true);	
    	
		settings.onLoading = 1;
    	settings.navFreeze = 1;
		
        //reset the nav link current state
        settings.navList.each(function (n) { $(this).removeClass('current'); });
        $(settings.navList[index]).addClass('current');
        
		//Change the background image then display the new content
        var currentImage;
        if (settings.introActive == 1) {
            currentImage = $(settings.imageIntro);
        } else {
            currentImage = $(settings.imageList[settings.currentIndex]);
        }
        
        currentImage.fadeOut( "fast", function() {
    		
    		if(bStartImage!=1){
			    //- for clear memory
    			currentImage.attr('src', '');
			}
				
	        if (!currentImage.hasClass('default')) { currentImage.attr('class', 'previous'); }
	    	
			if(bStartImage!=1)
			    settings.prevImage = currentImage;
			else
				settings.prevImage = null;
			
			var nextImage = $(settings.imageList[index]);
			nextImage.attr('class', 'current');
			nextImage.attr('src', nextImage.attr('path'));
  		});
    };

    var loadModuleContent = function (settings) {
    	
    	var index = settings.currentIndex;
    	
        if (settings.introActive == 1) {
            $(settings.contentIntro).hide();
            $(settings.contentWrapper).attr('class', '');
        }

        //Resize content        
        if (settings.contentResize == 1 && parseInt($(settings.contentWrapper).height()) != parseInt($(settings.contentList[index]).height() + 10)) {
            $(settings.contentWrapper).animate({
                height: $(settings.contentList[index]).height() + 10
            }, settings.contentResizeSpeed, function () {
               loadModuleContentAction(settings, index);
            });
        } else {
            loadModuleContentAction(settings, index);
        }
				
        //update counter for previous/next nav
        $(settings.navWrapper).find('.counter_current').text(index + 1);
    };

    var loadModuleContentAction = function (settings, index) {
        //display the loaded content                
        $(settings.contentList).hide();
        $(settings.contentList[index]).show();

        if (settings.contentLinksWrapper != null) {
            $(settings.contentLinksList).removeClass('current');
            $(settings.contentLinksList[index]).addClass('current');
        }
    };

	var showLoading = function(bShow){
    	if(bShow){
    		var window_width = $(window).width();
			var window_height = $(window).height();
    		$(".barousel_loading").css("left", ( window_width - $(".barousel_loading").width() )/2);
    		$(".barousel_loading").css("top", ( window_height - $(".barousel_loading").height() )/2);
    		$(".barousel_loading").show();
    		$(".barousel_exif_data").hide();
    	}
    	else
    		$(".barousel_loading").hide();
    };
	
	var showExifInfo = function(settings, oImage, bshow){
		var exif_data = settings.exifData;
		var stble = settings.stringTable;
		var showData = function(data){
			if(data==undefined) return "";
			return data;
		};
		
		if(!exif_data||!bshow){
			$(".barousel_exif_data").hide();
			return;
		}
		
		var make = showData(exif_data["Make"]);
	    var model = showData(exif_data["Model"]);
	    var t = showData(exif_data["ExposureTime"]);
	    var f = showData(exif_data["FNumber"]);
	    var length = showData(exif_data["FocalLength"]);
	    var iso = showData(exif_data["ISOSpeedRatings"]);
	    var ev = showData(exif_data["ExposureBias"]);
	    var soft = showData(exif_data["Software"]);
	    var date = showData(exif_data["DateTime"]);
	    var dpi = showData(exif_data["XResolution"]);
	    var sa = showData(exif_data["Saturation"]);
	    var sha = showData(exif_data["Sharpness"]);
	    var wb = showData(exif_data["WhiteBalance"]);
	   	
	   	if(t!=""){		
			if ( t < 1 ) {   
				t = "1/" + Math.round( 1 / t ) + "sec";  
			} else {   
				t = t + "sec";
			}
		}
		
	    var evnum = new Number( ev ) ;   
	    ev = evnum.toFixed( 1 ) ;       
	   
	    var showExit =  stble.getString("title_exif_make") + ": " + make + "<br>" +
	    				stble.getString("title_exif_model") + ": " + model + "<br>" +
	    				stble.getString("title_exif_shutter") + ": " + t + "<br>" +
	    				stble.getString("title_exif_aperture") + ": " + f + "<br>" +
	    				"ISO: " + iso + "<br>" +
	    				stble.getString("title_exif_focal_length") + ": " + length + "mm<br>" +
	    				stble.getString("title_exif_exposure") + ": " + ev + "<br>" +
	    				stble.getString("title_exif_wb") + ": " + wb + "<br>" +
	    				stble.getString("title_exif_saturation") + ": " + sa + "<br>" +
	    				stble.getString("title_exif_sharpness") + ": " + sha + " <br>" +
	    				stble.getString("title_exif_software") + ": " + soft + "<br>" +
	    				stble.getString("title_exif_resolution") + ": " + dpi + "DPI<br>" +
	    				stble.getString("title_exif_date") + ": " + date; 
	    					 
	   	$(".barousel_exif_data").html( showExit.replace( new RegExp( String.fromCharCode(0), "g" ), '' ));  
	   	$(".barousel_exif_data").show();
	};
	
	var trace = function(msg){
		if(console.log)
			console.log(msg);
	};
})(jQuery);