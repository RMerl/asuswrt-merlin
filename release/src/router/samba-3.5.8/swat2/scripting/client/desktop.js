/*
   Windows, tabs, and general widgetry for SWAT.

   Copyright (C) Deryck Hodge 2005
   released under the GNU GPL Version 3 or later
*/

/* Qooxdoo's browser sniffer doesn't distinguish IE version.
We'll cover IE 6 for now, but these checks need to be
revisited for fuller browser coverage. */
var browser = QxClient().engine;

// DocX/Y returns the width/height of the page in browser
function docX()
{
	var x;
	if (browser != "mshtml") {
		x = window.innerWidth;
	} else {
		x = document.documentElement.clientWidth;
	}
	return x;
}

function docY()
{
	var y;
	if (browser != "mshtml") {
		y = window.innerHeight;
	} else {
		y = document.documentElement.clientHeight;
	}
	return y;
}

//  If given a number, sizeX/Y returns in pixels a percentage of the browser
//  window. If given a Window object, sizeX/Y returns the size of that object. 
function sizeX(s)
{
	var sX;

	if (typeof(s) == 'number') {
		sX = Math.floor(docX() * s);
	} else {
		sX = s.getMinWidth();
	}

	return sX;
}

function sizeY(s)
{
	var sY;
	if (typeof(s) == 'number') {
		sY = Math.floor(docY() * s);
	} else {
		sY = s.getMinHeight();
	}

	return sY;
}

function getPosX(win)
{
	var y = Math.floor( (docY() - sizeY(win)) * Math.random() );
	return y;
}

function getPosY(win)
{
	var x = Math.floor( (docX() - sizeX(win)) * Math.random() );
	return x;
}

function openIn(e)
{
	var blank = new Window("New Menu");
	e.add(blank);
	blank.setVisible(true);
}
	
function Window(h, src, s)
{
	this.self = new QxWindow(h);

	// Settings for all windows
	if (s) {
		this.self.setMinWidth(sizeX(s));
		this.self.setMinHeight(sizeY(s));
	}
	this.self.setTop(getPosX(this.self));
	this.self.setLeft(getPosY(this.self));

	this.self.addEventListener("contextmenu", contextMenu);

	return this.self;
}

function SmallWindow(h, src)
{
	this.self = new Window(h, src, .20);
	return this.self;
}

function StandardWindow(h, src)
{
	this.self = new Window(h, src, .45);
	return this.self;
}

function LargeWindow(h, src)
{
	this.self = new Window(h, src, .70);
	return this.self;
}

Window.small = SmallWindow;
Window.standard = StandardWindow;
Window.large = LargeWindow;


