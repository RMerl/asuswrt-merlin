/*=============================    
jquery.barousel.js 
v.0.1
Julien Decaudin - 03/2010
www.juliendecaudin.com
=============================*/

(function ($) {
		
    $.fn.barousel = function (callerSettings) {
    	
        var settings = $.extend({
            imageWrapper: '.barousel_image',
            contentWrapper: '.barousel_content',
            contentLinksWrapper: null,
            navWrapper: '.barousel_nav',
            slideDuration: 3000, //duration of each slide in milliseconds
            navType: 1, //1: boxes navigation; 2: prev/next navigation; 3: custom navigation
            fadeIn: 1, //fade between slides; activated by default
            fadeInSpeed: 500, //fade duration in milliseconds 
            manualCarousel: 0, //manual carousel if set to 1
            contentResize: 1, //resize content container
            contentResizeSpeed: 300, //content resize speed in milliseconds
            debug: 0,
            startIndex: 0,
            hideNavBarDuration: 3000,
            onLoading: 0
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
				
		$(window).resize(function(){
			var currentImage = settings.imageList[settings.currentIndex];
			resizeImage(settings, currentImage, settings.currentIndex);
			//console.log("jquery.barousel->resize...settings.currentIndex="+settings.currentIndex);
		});
					  
        if (settings.imageWrapper.find('img[class*=intro]').length > 0) {
            settings.introActive = 1;
        }
				
		//set the index of each image  
        settings.imageList.each(function (n) {
        	this.onload = function(e){
				settings.imageSize[settings.currentIndex] = { width:$(this).width(),
															  height:$(this).height() };
        			
        		resizeImage(settings, $(this).get(0), settings.currentIndex); 
        			
        		settings.onLoading = 0;
        			
        		currentImage = $(settings.imageList[settings.currentIndex]);
        			
        		if (settings.fadeIn == 0) {
        			currentImage.show();
        				
        			if(settings.prevImage){
			      		settings.prevImage.hide();
		        	}
        		}
        		else{
	        		settings.navFreeze = 1;
			       	currentImage.fadeIn(settings.fadeInSpeed, function () {
						if(settings.prevImage){
							settings.prevImage.hide();	          	 
							settings.prevImage.removeClass('previous');
						}
						settings.navFreeze = 0;	             
			       	});
	          	}
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
            if (settings.debug == 1) console.log('[Barousel error] images and contents must be the same number');
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
        if (settings.navType == 1) {
        	//items navigation type
            var strNavList = "<ul>";
            settings.imageList.each(function (n) {
            	var currentClass = "";
                if (n == 0) currentClass = "current";
                strNavList += "<li><a href='#' title='" + $(settings.contentList[n]).find('p.header').text() + "' class='" + currentClass + "'>&nbsp;</a></li>";
            });
            strNavList += "</ul>";
            settings.navWrapper.append(strNavList);
            settings.navList = settings.navWrapper.find('a'); //list of the items' nav link

            //set the index of each nav link
            settings.navList.each(function (n) { this.index = n; });

        } 
        else if (settings.navType == 2) {
            var strNavList = "";
            strNavList += "<div class='counter'><span class='counter_current'>1</span>/<span class='counter_total'>" + settings.totalItem + "</span></div>";
               
            if (settings.totalItem > 1){
             	strNavList += "<div class='nav_btn nav_btn_icon prev transparent'><a href='#' title='prev'>&nbsp;</a></div>";
             	strNavList += "<div class='nav_btn nav_btn_icon next transparent'><a href='#' title='next'>&nbsp;</a></div>";
             	strNavList += "<div class='nav_btn_icon play transparent'><a href='#' title='play'>&nbsp;</a></div>";
             	strNavList += "<div class='nav_btn_icon pause transparent'><a href='#' title='pause'>&nbsp;</a></div>";
            }
                
            strNavList += "<div class='nav_btn nav_btn_icon close transparent'><a href='#' title='close'>&nbsp;</a></div>";
                
            settings.navWrapper.append(strNavList);
            settings.navList = settings.navWrapper.find('a'); //list of the items' nav link
                
            $(".play").css("display", "block");
            $(".pause").css("display", "none");
                
        } 
        else if (settings.navType == 3) {
            //custom navigation [static build]
            settings.navList = settings.navWrapper.find('a'); //list of the items' nav link
            //set the index of each nav link
            settings.navList.each(function (n) { this.index = n; });
        }

        //init the navigation click event
        if (settings.navType == 1 || settings.navType == 3) {
            //items navigation type
            settings.navList.each(function (n) {
            	$(this).click(function () {
                	if (settings.navFreeze == 0) {
                    	window.clearTimeout(settings.timerCarousel);
                        settings.stopCarousel = 1;
                        if (settings.currentIndex != n || settings.introActive == 1) {
                        	loadItem(settings, n);
                            settings.currentIndex = n;
                        }
                    }
                    settings.introActive = 0;
                    return false;
                });
            });
        } 
        else if (settings.navType == 2) {
			//prev/next navigation type
            settings.navList.each(function () {
            	$(this).click(function () {
                	if (settings.navFreeze == 0) {
                    	window.clearTimeout(settings.timerCarousel);
                       	settings.stopCarousel = 1;

                       	if ($(this).parent().hasClass('prev')) {
                       		var previousIndex;
											
                          	if (parseInt(settings.currentIndex) == 0) {
                          		previousIndex = parseInt(settings.totalItem) - 1;
                          	} else {
                            	previousIndex = parseInt(settings.currentIndex) - 1;
                          	}
                          	loadItem(settings, previousIndex);
                          	settings.currentIndex = previousIndex;
                       } 
                       else if ($(this).parent().hasClass('next')) {
                          	var nextIndex;
																
                          	if (parseInt(settings.currentIndex) == (parseInt(settings.totalItem) - 1)) {
                            	nextIndex = 0;
                          	} else {
                            	nextIndex = parseInt(settings.currentIndex) + 1;
                          	}
                          	loadItem(settings, nextIndex);
                          	settings.currentIndex = nextIndex;
                       } 
                       else if ($(this).parent().hasClass('close')) {
                            $(".barousel").remove();
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
        }
						
        //start the carousel
        if (settings.manualCarousel == 0) {
            var loadItemCall = function () { loadItem(settings, 1); };
            settings.timerCarousel = window.setTimeout(loadItemCall, settings.slideDuration);
        }
        else{
            loadItem(settings, settings.startIndex, 1);
        }
       	
        return this;
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
		
	var resizeImage = function(settings, ImgD, index) {
		
		var window_width = $(window).width();
		var window_height = $(window).height();
			
		var imgWidth = settings.imageSize[index].width;
		var imgHeight = settings.imageSize[index].height;
		
		if(imgWidth<=0 || imgHeight<=0)
			return;
		
		var newImgWidth = imgWidth;
		var newImgHeight = imgHeight;
		if( newImgWidth > window_width )	{
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
	};
								
    var loadItem = function (settings, index, bStartImage) {
    		
    		if( settings.onLoading == 1 ){
    			return;
    		}
    		
        //reset the nav link current state
        if (settings.navType != 2) {
            settings.navList.each(function (n) { $(this).removeClass('current'); });
            $(settings.navList[index]).addClass('current');
        }
				
				
				
        //Change the background image then display the new content
        var currentImage;
        if (settings.introActive == 1) {
            currentImage = $(settings.imageIntro);
        } else {
            currentImage = $(settings.imageList[settings.currentIndex]);
        }
        
        var nextImage = $(settings.imageList[index]);
				
        if (!currentImage.hasClass('default')) { currentImage.attr('class', 'previous'); }
        nextImage.attr('class', 'current');
				
				var already_load = 0;
				
				if(nextImage.attr('src')==''){
					nextImage.hide();
					settings.onLoading = 1;
					nextImage.attr('src', nextImage.attr('path'));
				}
				else{
					//- Already load
					resizeImage(settings, settings.imageList[index], index);
					already_load = 1;
				}
								
				if(already_load==1){
	        //fade-in effect
		      if (settings.fadeIn == 0) {
		          nextImage.show();
		          if(bStartImage!=1) currentImage.hide();
		          loadModuleContent(settings, index);
		      } 
		      else {
		      		
		          settings.navFreeze = 1;
		          $(settings.contentList).hide();
		          nextImage.fadeIn(settings.fadeInSpeed, function () {	          	 
		          	 if(bStartImage!=1) currentImage.hide();	          	 
		             currentImage.removeClass('previous');
		             settings.navFreeze = 0;	             
		          });
		          
		          $(settings.contentList).hide();
		          
		          if(bStartImage!=1)
		          	settings.prevImage = currentImage;
		          else
		          	settings.prevImage = null;
		             
		      		loadModuleContent(settings, index);
		      }
				}
				else{
					if(bStartImage!=1)
		      	settings.prevImage = currentImage;
		      else
		        settings.prevImage = null;
		      
		      $(settings.contentList).hide();
		      loadModuleContent(settings, index);
		    }
	      	
        //carousel functionnality (deactivated when the user click on an item)
        if (settings.stopCarousel == 0) {
            settings.currentIndex = index;
            var nextIndex;

            if (settings.currentIndex == settings.totalItem - 1) {
                nextIndex = 0;
            } else {
                nextIndex = parseInt(settings.currentIndex) + 1;
            }
            
            if (settings.manualCarousel == 0){
            	var loadItemCall = function () { 
            		loadItem(settings, nextIndex); 
            	};
            	settings.timerCarousel = window.setTimeout(loadItemCall, settings.slideDuration);
          	}
        }
    };

    var loadModuleContent = function (settings, index) {
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
        if (settings.navType == 2) {
            $(settings.navWrapper).find('.counter_current').text(index + 1);
        }
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

})(jQuery);