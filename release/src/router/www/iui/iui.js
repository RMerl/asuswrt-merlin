/*
   copyright:
   Copyright (c) 2007-12, iUI Project Members.
   See LICENSE.txt for licensing terms.
   Version 0.4
 */

(function() {

var slideSpeed = 20;			// Slide percentage per interval
var slideInterval = 0;			// Intervals are as fast as possible for mobile, should be slower for desktop
var ajaxTimeoutVal = 30000;

var originalPage = null;
var currentPage = null;
var currentDialog = null;
var currentWidth = 0;
var currentHeight = 0;
var currentHash = location.hash;
var hashPrefix = "#_";
var pageHistory = [];			// Navigation stack (poorly named, different from browser history)
var newPageCount = 0;
var checkTimer;
var hasOrientationEvent = false;
var portraitVal = "portrait";
var landscapeVal = "landscape";

// *************************************************************************************************

/*
events:
iUI fires a number of custom events on your panel and dialog elements. Handling
these events is the recommended way to do any just-in-time transformations or
loading (besides the ajax pre-loading built into iUI).
*/

window.iui =
{
	/*
	property: iui.logging
	This is set to `true` logging (with console.log) is enabled.
	*/
	logging: false,

	/*
	property: iui.busy
	This is set to `true` if a slide animation is in progress.
	*/
	busy: false,
	
	/*
	property: iui.animOn
	Determines whether to do horizontal slide animations with CSS transitions
	(http://www.w3.org/TR/css3-2d-transforms/) where supported (defaults	to
	`true`). Otherwise, manual `setInterval()` style animations are performed
	(vertical slide animations are always done manually).
	*/
	animOn: true,
	
	/*
	property: iui.ajaxErrHandler
	If defined, this user-set function will be called when an AJAX call returns
	with an HTTP status other than `200` (currently all HTTP statuses other than
	`200`, even including 200-level statuses like `201 Created`, are seen as
	errors.  A status of `0` is treated as success for file:// URLs).
	*/
	ajaxErrHandler : null,
	
	/*
	property: iui.httpHeaders
	An object defining headers to be sent with Ajax requests. This defaults to:
	
	example:
	  { 'X-Requested-With': 'XMLHttpRequest' }
	*/
	httpHeaders: {
	    "X-Requested-With" : "XMLHttpRequest"
	},

	/*
	method: iui.showPage(page[, backwards=false])
	`showPage()` should probably be an internal function, outside callers should
	call `showPageById()` instead. `showPage()` does NOT set the busy flag because
	it is already set by the public-facing functions.
	
	`page` is the html element to show. If `backwards` is set to `true`, it will
	display a right-to-left animation instead of the default left-to-right.
	
	If the currently-displayed page is passed, iui will do nothing. `showPage()`
	is used for both panel-type pages and dialog-type pages (dialogs float on top
	of the panels, have a cancel button and do not participate in sliding
	animations). Panel-type pages receive blur/focus events and load/unload events,
	but dialog-type pages only receive blur/focus events.
	*/	
	showPage: function(page, backwards)
	{
		if (page)
		{
			if (page == currentPage)
			{
				log("page = currentPage = " + page.id);
				iui.busy = false;	//  Don't do anything, just clear the busy flag and exit
				return;
			}
			
			if (currentDialog)
			{
				currentDialog.removeAttribute("selected");
				sendEvent("blur", currentDialog);					// EVENT: BLUR
				currentDialog = null;
			}

			/*
			events:
			Dialogs receive a `focus` event when they are shown and a `blur` event
			when hidden. Currently they don't receive any `load` or `unload` events.
			*/
			if (hasClass(page, "dialog"))
			{
				iui.busy = false;	// There's no slide transition, so clear busy flag
				// There's no LOAD/UNLOAD events for dialogs -- is that the way it should be??
				// Should the view the dialog is going over get a BLUR??
				sendEvent("focus", page);							// EVENT: FOCUS
				showDialog(page);
			}
			/*
			events:
			Panels receive `focus` and `blur` events and also receive a `load` event
			and (only when going backwards away from a panel) an `unload` event.
			*/
			else
			{
				sendEvent("load", page);    						// EVENT: LOAD
													// 127(stylesheet), 128(script), 129(onload)
													// 130(onFocus), 133(loadActionButton)
				var fromPage = currentPage;
				sendEvent("blur", currentPage);						// EVENT: BLUR
				currentPage = page;
				sendEvent("focus", page);							// EVENT: FOCUS

				if (fromPage)
				{
					setTimeout(slidePages, 0, fromPage, page, backwards);
				}
				else
				{
					updatePage(page, fromPage);
				}
					
			}
		}
	},


	gotoView: function(view, replace)
	{
		var node, nodeId;
		
		if (view instanceof HTMLElement)
		{
			node = view;
			nodeId = node.id;
		}
		else
		{
			nodeId = view;
			node = $(nodeId);
		}
		if (!node) log("gotoView: node is null");
		if (!iui.busy)
		{
			iui.busy = true;
			var index = pageHistory.indexOf(nodeId);
			var backwards = index != -1;
			if (backwards)
			{
				// we're going back, remove history from index on
				// remember - pageId will be added again in updatePage
				pageHistory.splice(index);
			}
			else if (replace)
			{
				pageHistory.pop();				
			}
			iui.showPage(node, backwards);
			return false;
		}
		else
		{
			return true;
		}
	},


	/*
	method: iui.showPageById(pageId)
	Looks up the page element by the id and checks the internal history to
	determine if the page is on the stack -- if so, it will call `showPage()` with
	`backwards` set to `true`, reversing the direction of the animation. 
	*/
	showPageById: function(pageId)
	{
		iui.gotoView(pageId, false);
	},

	/*
	method: iui.goBack()
	Navigates to the previous page in the history stack.
	*/
	goBack: function()
	{
	    iui.gotoView(pageHistory.slice(-2,-1)[0], false);
	},


	/*
	method: iui.replacePage(pageId)
	Loads a new page at the same level in the history stack. 
	Currently it will do a slide-in animation, but replaces
	the current page in the navStack.
	It should probably use a different animation (slide-up/slide-down).
	*/
	replacePage: function(view)
	{
	    gotoView(view, true);
	},

	/*
	method: iui.showPageByHref(href, args, method, replace, cb)
	Outside callers should use this version to do an ajax load programmatically
	from your webapp.
	
	`href` is a URL string, `method` is the HTTP method (defaults to `GET`),
	`args` is an Object of key-value pairs that are used to generate the querystring,
	`replace` is an existing element that either is the panel or is a child of the
	panel that the incoming HTML will replace (if not supplied, iUI will append
	the incoming HTML to the `body`), and `cb` is a user-supplied callback function.
	*/
	showPageByHref: function(href, args, method, replace, cb)
	{
	  // I don't think we need onerror, because readyState will still go to 4 in that case
		function spbhCB(xhr) 
		{
			log("xhr.readyState = " + xhr.readyState);
			if (xhr.readyState == 4)
			{
				if ((xhr.status == 200 || xhr.status == 0) && !xhr.aborted)
				{
				  // Add 'if (xhr.responseText)' to make sure we have something???
				  // Can't use createDocumentFragment() here because firstChild is null and childNodes is empty
				  var frag = document.createElement("div");
				  frag.innerHTML = xhr.responseText;
				  // EVENT beforeInsert->body
					/*
					events:
					When new pages are inserted into the DOM after an AJAX load, the `body`
					element receives a `beforeinsert` event with `{ fragment: frag }` parameters
					and afterwards receives an `afterinsert` event with `{insertedNode: docNode}` parameters.
					*/
				  sendEvent("beforeinsert", document.body, {fragment:frag})
				  if (replace)
				  {
					  replaceElementWithFrag(replace, frag);
					  iui.busy = false;
				  }
				  else
				  {
					  iui.insertPages(frag);
				  }
				}
				else
				{
					iui.busy = false;
					if (iui.ajaxErrHandler)
					{
						iui.ajaxErrHandler("Error contacting server, please try again later");
					}
				}
				if (cb)
				{
					setTimeout(cb, 1000, true);
				}
			}
		  
		};
		if (!iui.busy)
		{
			iui.busy = true;
			iui.ajax(href, args, method, spbhCB);
		}
		else 
		{
			cb();	// We didn't get the "lock", we may need to unselect something
		}
	},
	
	/*
	method: iui.ajax(url, args, method, cb)
	Handles ajax requests and also fires a `setTimeout()` call
	to abort the request if it takes longer than 30 seconds. See `showPageByHref()`
	above for a description of the various arguments (`url` is the same as `href`).
	*/
	ajax: function(url, args, method, cb)
	{
        var xhr = new XMLHttpRequest();
        method = method ? method.toUpperCase() : "GET";
        if (args && method == "GET")
        {
          url =  url + "?" + iui.param(args);
        }
        xhr.open(method, url, true);
        if (cb)
        {
			xhr.onreadystatechange = function() { cb(xhr); };
        }
        var data = null;
        if (args && method != "GET")
        {
            xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
            data = iui.param(args);
        }
        for (var header in iui.httpHeaders)
        {
            xhr.setRequestHeader(header, iui.httpHeaders[header]);
        }
        xhr.send(data);
        xhr.requestTimer = setTimeout( ajaxTimeout, ajaxTimeoutVal );
		return xhr;
        function ajaxTimeout()
        {
			try{
		 		xhr.abort();
		   		xhr.aborted = true;
			}
		   	catch(err){
				log(err);
		 	}
		}
	},
	
	/*
	method: iui.param(o)
	Stripped-down, simplified object-only version of a jQuery function that
	converts an object of keys/values into a URL-encoded querystring.
	*/
	param: function( o )
	{
	  var s = [ ];
	
	  // Serialize the key/values
	  for ( var key in o )
	  {
	    var value = o[key];
	    if (typeof(value) == "object" && typeof(value.length) == "number")
	    {
	        for (var i = 0; i < value.length ; i++)
	        {
		        s[ s.length ] = encodeURIComponent(key) + '=' + encodeURIComponent(value[i]);
		    }
	    }
	    else
	    {
		    s[ s.length ] = encodeURIComponent(key) + '=' + encodeURIComponent(value);
		}
      }
  
	  // Return the resulting serialization
	  return s.join("&").replace(/%20/g, "+");
	},

	/*
	method: iui.insertPages(frag)
	If an AJAX call (`showPageByHref()`) is made without supplying a `replace`
	element, `insertPages()` is called to insert the newly-created element
	fragment into the page DOM. Each child-node of the HTML fragment is a panel
	and if any of them are already in the DOM, they will be replaced by the
	incoming elements.
	*/
	insertPages: function(frag)
	{
		var nodes = frag.childNodes;
		var targetPage;
		for (var i = 0; i < nodes.length; ++i)
		{
			var child = nodes[i];
			if (child.nodeType == 1)
			{
				if (!child.id)
					child.id = "__" + (++newPageCount) + "__";

				var clone = $(child.id);
				var docNode;
				if (clone) {
					clone.parentNode.replaceChild(child, clone);
				    docNode = $(child.id);
			    }
				else
					docNode = document.body.appendChild(child);
					
				sendEvent("afterinsert", document.body, {insertedNode:docNode});   

				// First child becomes selected page/view by default unless
				// selected="true" is set
				// BUG: selected="true" results in a visually incorrect transition
				if (child.getAttribute("selected") == "true" || !targetPage)
					targetPage = child;
				
				--i;
			}
		}
		sendEvent("afterinsertend", document.body, {fragment:frag})

		if (targetPage)
			iui.showPage(targetPage);

	},

	/*
	method: iui.getSelectedPage()
	Returns the panel element that is currently being viewed. Each panel must be a
	direct child of the `body` element. A panel is set as the selected panel by
	setting the `selected` attribute to `true`.
	*/
	getSelectedPage: function()
	{
		for (var child = document.body.firstChild; child; child = child.nextSibling)
		{
			if (child.nodeType == 1 && child.getAttribute("selected") == "true")
				return child;
		}	 
	},
	
	/*
	method: iui.getAllViews()
	Returns all panels -- currently requires querySelectorAll() will be fixed
	*/
	getAllViews: function()
	{
		return document.querySelectorAll("body > *:not(.toolbar)");
	},
	
	/*
	method: iui.isNativeUrl(href)
	Determines whether the supplied URL string launches a native iPhone app (maps,
	YouTube, phone, email, etc). If so, iUI does nothing (doesn't attempt to load
	a page or slide to it) and allows the phone to handle it the click natively.
	*/
	isNativeUrl: function(href)
	{
		for(var i = 0; i < iui.nativeUrlPatterns.length; i++)
		{
			if(href.match(iui.nativeUrlPatterns[i])) return true;
		}
		return false;
	},
	nativeUrlPatterns: [
		new RegExp("^http:\/\/maps.google.com\/maps\?"),
		new RegExp("^mailto:"),
		new RegExp("^tel:"),
		new RegExp("^http:\/\/www.youtube.com\/watch\\?v="),
		new RegExp("^http:\/\/www.youtube.com\/v\/"),
		new RegExp("^javascript:"),
		new RegExp("^sms:"),
		new RegExp("^callto:")
	],
	/*
	method: iui.hasClass(self, name)
	Convenience function to determine if the given element (`self`) has the
	class `name`.
	*/
	hasClass: function(self, name)
	{
		var re = new RegExp("(^|\\s)"+name+"($|\\s)");
		return re.exec(self.getAttribute("class")) != null;
	},
	
	/*
	method: iui.addClass(self, name)
	Convenience function to add the given class `name` to element `self`.
	*/	
	addClass: function(self, name)
	{
	  if (!iui.hasClass(self,name)) self.className += " "+name;
	},
		
	/*
	method: iui.removeClass(self, name)
	Convenience function to remove the given class `name` to element `self`.
	*/
	removeClass: function(self, name)
	{
	  if (iui.hasClass(self,name)) {
		  var reg = new RegExp('(\\s|^)'+name+'(\\s|$)');
		self.className=self.className.replace(reg,' ');
	  }
	}
};

// *************************************************************************************************

/*
load: On Load
On load, iUI will determine which page to display primarily based on
the anchor part of the URL (everything after `#_`) and secondarily based on the
top-level (child of the `body`) element with the `selected` attribute set to
`true`. If these both exist, iui.showPage() will be called twice, but the
anchor-based load will win because it is done second.
*/
addEventListener("load", function(event)
{
	var page = iui.getSelectedPage();
	var locPage = getPageFromLoc();
		
	if (page)
	{
		originalPage = page;
		iui.showPage(page);
	}
	
	if (locPage && (locPage != page))
		iui.showPage(locPage);
	
	setTimeout(preloadImages, 0);
	if (typeof window.onorientationchange == "object")
	{
		window.onorientationchange=orientChangeHandler;
		hasOrientationEvent = true;
		setTimeout(orientChangeHandler, 0);
	}
	setTimeout(checkOrientAndLocation, 0);
	checkTimer = setInterval(checkOrientAndLocation, 300);
}, false);

addEventListener("unload", function(event)
{
	return;
}, false);
	
/*
click: Link Click Handling
iUI captures all clicks on `a` elements and goes through a series of checks to
determine what to do:

1. If the link has a `href="#..."`, iUI will navigate to the panel ID specified
   after the # (no underscore).
2. If the link's ID is `backButton`, iUI will navigate to the previous screen
   (see `iui.goBack()`).
3. If the link has a `type="submit"`, iUI will find the parent `form` element,
   gather up all the input values and submit the form via AJAX (see
   `iui.showPageByHref()`).
4. If the link has a `type="cancel"`, iUI will cancel the parent `form` element
   dialog.
5. If the link has a `target="_replace"`, iUI will do an AJAX call based on the
   href of the link and replace the panel that the link is in with the contents
   of the AJAX response.
6. If the link is a native URL (see `iui.isNativeURL()`), iUI will do nothing.
7. If the link has a `target="_webapp"`, iUI will perform a normal link,
   navigating completely away from the iUI app and pointing the browser to the
   linked-to webapp instead.
8. If there is no `target` attribute, iUI will perform a normal (non-replace)
   AJAX slide (see `iui.showPageByHref()`).
*/
addEventListener("click", function(event)
{
	var link = findParent(event.target, "a");
	if (link)
	{
		function unselect() { link.removeAttribute("selected"); }
		if (link.href && link.hash && link.hash != "#" && !link.target)
		{
			followAnchor(link);
		}
		else if (link == $("backButton"))
		{
			iui.goBack();
		}
		else if (link.getAttribute("type") == "submit")
		{
			/* Forms with a[type=submit] links are deprecated
			 * this code will be removed in a future release.
			 */
			var form = findParent(link, "form");
			if (form.target == "_self")
			{
				/* Browser submit (with full-page response) */
				if (typeof form.onsubmit == 'function')
				{
                    if (form.onsubmit() == true)
                    {
                    	form.submit();
                    }
                }
                else
                {
                    form.submit();
                }
			    return;  // allow default
			}
			submitForm(form);	// Ajax submit
		}
		else if (link.getAttribute("type") == "cancel")
		{
			cancelDialog(findParent(link, "form"));
		}
		else if (link.target == "_replace")
		{
			followAjax(link, link);
		}
		else if (iui.isNativeUrl(link.href))
		{
			return;
		}
		else if (link.target == "_webapp")
		{
			location.href = link.href;
		}
		else if (!link.target && link.href)
		{
			followAjax(link, null);
		}
		else
			return;
		
		event.preventDefault();		   
	}
}, true);

/*
click: Div.toggle Click Handling
iUI also captures `div.toggle` clicks and displays/hides the element via setting
a `toggled` attribute to true/false.
*/
addEventListener("click", function(event)
{
	var div = findParent(event.target, "div");
	if (div && hasClass(div, "toggle"))
	{
		div.setAttribute("toggled", div.getAttribute("toggled") != "true");
		event.preventDefault();		   
	}
}, true);

/*
click: input[submit] Click Handling
Add an attribute to an input[submit] so we can send the right value to the server.
*/
addEventListener("click", function(event)
{
    var input = findParent(event.target, "input");
    if (input && input.type == "submit")
    {
		input.setAttribute("submitvalue", input.value);
    }
}, true);

/*
submit: Form submit handling
All forms without target="_self" will use iUI's Ajax from submission.
*/
addEventListener("submit", function(event)
{
    var form = event.target;
	if (form.target != "_self")
	{
		event.preventDefault();
    	submitForm(form);
    }
}, true);

function followAnchor(link)
{
	link.setAttribute("selected", "true");
	var busy = iui.gotoView(link.hash.substr(1), false);
	// clear selected immmediately if busy, else wait for transition to finish
	setTimeout(function() {link.removeAttribute("selected")}, busy ? 0 : 500);   
}

function followAjax(link, replaceLink)
{
	link.setAttribute("selected", "progress");
	iui.showPageByHref(link.href, null, "GET", replaceLink, function() { link.removeAttribute("selected"); });	
}

function sendEvent(type, node, props)
{
    if (node)
    {
        var event = document.createEvent("UIEvent");
        event.initEvent(type, false, false);  // no bubble, no cancel
        if (props)
        {
            for (i in props)
            {
                event[i] = props[i];
            }
        }
        node.dispatchEvent(event);
    }
}

function getPageFromLoc()
{
	var page;
	var result = location.hash.match(/#_([^\?_]+)/);
	if (result)
		page = result[1];
	if (page)
		page = $(page);
	return page;
}

function orientChangeHandler()
{
	var orientation=window.orientation;
	switch(orientation)
	{
	case 0:
		setOrientation(portraitVal);
		break;	
		
	case 90:
	case -90: 
		setOrientation(landscapeVal);
		break;
	}
}


function checkOrientAndLocation()
{
	if (!hasOrientationEvent)
	{
	  if ((window.innerWidth != currentWidth) || (window.innerHeight != currentHeight))
	  {	  
		  currentWidth = window.innerWidth;
		  currentHeight = window.innerHeight;
		  var orient = (currentWidth < currentHeight) ? portraitVal : landscapeVal;
		  setOrientation(orient);
	  }
	}

	if (location.hash != currentHash)
	{
		var pageId = location.hash.substr(hashPrefix.length);
		if ((pageId == "") && originalPage)	// Workaround for WebKit Bug #63777
			pageId = originalPage.id;
		iui.showPageById(pageId);
	}
}

function setOrientation(orient)
{
	document.body.setAttribute("orient", orient);
//  Set class in addition to orient attribute:
	if (orient == portraitVal)
	{
		iui.removeClass(document.body, landscapeVal);
		iui.addClass(document.body, portraitVal);
	}
	else if (orient == landscapeVal)
	{
		iui.removeClass(document.body, portraitVal);
		iui.addClass(document.body, landscapeVal);
	}
	else
	{
		iui.removeClass(document.body, portraitVal);
		iui.removeClass(document.body, landscapeVal);
	}
	setTimeout(scrollTo, 100, 0, 1);
}

function showDialog(page)
{
	currentDialog = page;
	page.setAttribute("selected", "true");
	
	if (hasClass(page, "dialog"))
		showForm(page);
}

function showForm(form)
{
	/* Noop click-handler on the page works around problem where
	   our main click handler doesn't get called in Mobile Safari */
	form.addEventListener("click",function(event){},true);
}

function cancelDialog(form)
{
	form.removeAttribute("selected");
}

function updatePage(page, fromPage)
{
	if (!page.id)
		page.id = "__" + (++newPageCount) + "__";

	currentHash = hashPrefix + page.id;
	if (!fromPage)
	{	// If fromPage is null, this is the initial load and we want to replace a hash of "" with "#_home" or whatever the initial page id is.
//		location.replace(location.protocol + "//" + location.hostname + location.port + location.pathname + newHash + location.search);
		location.replace(currentHash);
	}
	else
	{	// Otherwise, we want to generate a new history entry
//		location.hash = currentHash;
		location.assign(currentHash);
	}
		
	pageHistory.push(page.id);

	var pageTitle = $("pageTitle");
	if (page.title)
		pageTitle.innerHTML = page.title;
	var ttlClass = page.getAttribute("ttlclass");
	pageTitle.className = ttlClass ? ttlClass : "";

	if (page.localName.toLowerCase() == "form")
		showForm(page);
		
	var backButton = $("backButton");
	if (backButton)
	{
		var prevPage = $(pageHistory[pageHistory.length-2]);
		if (prevPage && !page.getAttribute("hideBackButton"))
		{
			backButton.style.display = "inline";
			backButton.innerHTML = prevPage.title ? prevPage.title : "Back";
			var bbClass = prevPage.getAttribute("bbclass");
			backButton.className = (bbClass) ? 'button ' + bbClass : 'button';
		}
		else
			backButton.style.display = "none";
	}
	iui.busy = false;
}
/*
events:
Both panels involved in a slide animation receive `beforetransition` and
`aftertransition` events. The panel being navigated from receives event
parameters `{ out :true }`, the panel being navigated to receives `{ out: false }`.
*/
function slidePages(fromPage, toPage, backwards)
{		 
	var axis = (backwards ? fromPage : toPage).getAttribute("axis");

	clearInterval(checkTimer);
	
	sendEvent("beforetransition", fromPage, {out:true});
	sendEvent("beforetransition", toPage, {out:false});
	if (canDoSlideAnim() && axis != 'y')
	{
	  slide2(fromPage, toPage, backwards, slideDone);
	}
	else
	{
	  slide1(fromPage, toPage, backwards, axis, slideDone);
	}

	function slideDone()
	{
	  if (!hasClass(toPage, "dialog"))
		  fromPage.removeAttribute("selected");
	  checkTimer = setInterval(checkOrientAndLocation, 300);
	  setTimeout(updatePage, 0, toPage, fromPage);
	  fromPage.removeEventListener('webkitTransitionEnd', slideDone, false);
	  sendEvent("aftertransition", fromPage, {out:true});
      sendEvent("aftertransition", toPage, {out:false});
	  if (backwards) sendEvent("unload", fromPage);	// EVENT: UNLOAD
	}
}

function canDoSlideAnim()
{
  return (iui.animOn) && (typeof WebKitCSSMatrix == "object");
}

function slide1(fromPage, toPage, backwards, axis, cb)
{
	if (axis == "y")
		(backwards ? fromPage : toPage).style.top = "100%";
	else
		toPage.style.left = "100%";

	scrollTo(0, 1);
	toPage.setAttribute("selected", "true");
	var percent = 100;
	slide();
	var timer = setInterval(slide, slideInterval);

	function slide()
	{
		percent -= slideSpeed;
		if (percent <= 0)
		{
			percent = 0;
			clearInterval(timer);
			cb();
		}
	
		if (axis == "y")
		{
			backwards
				? fromPage.style.top = (100-percent) + "%"
				: toPage.style.top = percent + "%";
		}
		else
		{
			fromPage.style.left = (backwards ? (100-percent) : (percent-100)) + "%"; 
			toPage.style.left = (backwards ? -percent : percent) + "%"; 
		}
	}
}


function slide2(fromPage, toPage, backwards, cb)
{
	toPage.style.webkitTransitionDuration = '0ms'; // Turn off transitions to set toPage start offset
	// fromStart is always 0% and toEnd is always 0%
	// iPhone won't take % width on toPage
	var toStart = 'translateX(' + (backwards ? '-' : '') + window.innerWidth +	'px)';
	var fromEnd = 'translateX(' + (backwards ? '100%' : '-100%') + ')';
	toPage.style.webkitTransform = toStart;
	toPage.setAttribute("selected", "true");
	toPage.style.webkitTransitionDuration = '';	  // Turn transitions back on
	function startTrans()
	{
		fromPage.style.webkitTransform = fromEnd;
		toPage.style.webkitTransform = 'translateX(0%)'; //toEnd
	}
	fromPage.addEventListener('webkitTransitionEnd', cb, false);
	setTimeout(startTrans, 0);
}

function preloadImages()
{
	var preloader = document.createElement("div");
	preloader.id = "preloader";
	document.body.appendChild(preloader);
}

function submitForm(form)
{
	iui.addClass(form, "progress");
	iui.showPageByHref(form.getAttribute('action'), encodeForm(form), form.hasAttribute('method') ? form.getAttribute('method') : 'GET', null, function() {iui.removeClass(form, "progress")});
}

function encodeForm(form)
{
	function encode(inputs)
	{
		for (var i = 0; i < inputs.length; ++i)
		{
			log("input[" + i + "]: " + inputs[i].name + " = " + inputs[i].value);
            if (inputs[i].name)
            {
            	var input = inputs[i];
            	if (input.getAttribute("type") == "checkbox" && !input.checked ||
					input.getAttribute("type") == "radio" && !input.checked ||
					input.disabled)
				{
            		continue;
            	}
            	if (input.getAttribute("type") == "submit")
            	{
            		if (input.getAttribute("submitvalue"))
					{	// Was marked, this is the value to send, but clear it for next time
						input.removeAttribute("submitvalue");
					}
					else
					{	// not marked, don't send value -- continue
						continue;
					}
				}
            	var value = args[input.name];
            	if (value === undefined)
            	{	// If parm is 'empty' just set it
					args[input.name] = input.value;
            	}
            	else if (value instanceof Array)
            	{	// If parm is array, add to it
					value.push(input.value);
            	}
            	else
            	{	// If parm is scalar, change to array and add to it
					args[input.name] = [value, input.value];
				}
            }
		}
	}

    var args = {};
    encode(form.getElementsByTagName("input"));
    encode(form.getElementsByTagName("textarea"));
    encode(form.getElementsByTagName("select"));
    encode(form.getElementsByTagName("button"));
    return args;	  
}

function findParent(node, localName)
{
	while (node && (node.nodeType != 1 || node.localName.toLowerCase() != localName))
		node = node.parentNode;
	return node;
}

function hasClass(self, name)
{
	return iui.hasClass(self,name);
}

function replaceElementWithFrag(replace, frag)
{
	var page = replace.parentNode;
	var parent = replace;
	while (page.parentNode != document.body)
	{
		page = page.parentNode;
		parent = parent.parentNode;
	}
	page.removeChild(parent);

    var docNode;
	while (frag.firstChild) {
		docNode = page.appendChild(frag.firstChild);
		sendEvent("afterinsert", document.body, {insertedNode:docNode});
    }
	sendEvent("afterinsertend", document.body, {fragment:frag})
}

function $(id) { return document.getElementById(id); }
function log() { if ((window.console!=undefined) && iui.logging) console.log.apply(console, arguments); }

})();
