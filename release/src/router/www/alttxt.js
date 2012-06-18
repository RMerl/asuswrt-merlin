/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  ALTTXT V1.2
//  BY: BRIAN GOSSELIN OF SCRIPTASYLUM.COM
//  ADDED FADING EFFECT FOR IE4+ AND NS6+ ONLY AND OPTIMIZED THE CODE A BIT.
//  SCRIPT FEATURED ON DYNAMIC DRIVE (http://www.dynamicdrive.com) 


var dofade=true;     // ENABLES FADE-IN EFFECT FOR IE4+ AND NS6 ONLY
var center=true;     // 將漸出說明欄出現在滑鼠中間(true), 否則出現在滑鼠右邊(false)
var centertext=false; // 說明文字以中間對準排列(true),否則文字靠左排列(false)


////////////////////////////// 以下無需修改 //////////////////////////////////////

var NS4 = (navigator.appName.indexOf("Netscape")>=0 && !document.getElementById)? true : false;
var IE4 = (document.all && !document.getElementById)? true : false;
var IE5 = (document.getElementById && document.all)? true : false;
var NS6 = (document.getElementById && navigator.appName.indexOf("Netscape")>=0 )? true: false;
var W3C = (document.getElementById)? true : false;

var OPR = (document.getElementById && navigator.appName.indexOf("Opera")>=0 )? true: false;

var w_y, w_x, navtxt, boxheight, boxwidth;
var ishover=false;
var isloaded=false;
var ieop=0;
var op_id=0;

function getwindowdims(){
w_y=(NS4||NS6||OPR)? window.innerHeight : (IE5||IE4)? document.body.clientHeight : 0;
w_x=(NS4||NS6||OPR)? window.innerWidth : (IE5||IE4)? document.body.clientWidth : 0;
}

function getboxwidth(){
if(NS4)boxwidth=(navtxt.document.width)? navtxt.document.width : navtxt.clip.width;
if(IE5||IE4)boxwidth=(navtxt.style.pixelWidth)? navtxt.style.pixelWidth : navtxt.offsetWidth;
if(NS6)boxwidth=(navtxt.style.width)? parseInt(navtxt.style.width) : parseInt(navtxt.offsetWidth);
if(OPR)boxwidth=(navtxt.style.width)? parseInt(navtxt.style.width) : parseInt(navtxt.offsetWidth);
}

function getboxheight(){
if(NS4)boxheight=(navtxt.document.height)? navtxt.document.height : navtxt.clip.height;
if(IE4||IE5)boxheight=(navtxt.style.pixelHeight)? navtxt.style.pixelHeight : navtxt.offsetHeight;
if(NS6)boxheight=parseInt(navtxt.offsetHeight);
if(OPR)boxheight=parseInt(navtxt.offsetHeight);
}

function movenavtxt(x,y){
if(NS4)navtxt.moveTo(x,y);
if(W3C||IE4){
navtxt.style.left=x+'px';
navtxt.style.top=y+'px';
}}

function getpagescrolly(){
if(NS4||NS6||OPR)return window.pageYOffset;
if(IE5||IE4)return document.body.scrollTop;
}

function getpagescrollx(){
if(NS4||NS6||OPR)return window.pageXOffset;
if(IE5||IE4)return document.body.scrollLeft;
}

function writeindiv(text){
if(NS4){
navtxt.document.open();
navtxt.document.write(text);
navtxt.document.close();
}
if(W3C||IE4)navtxt.innerHTML=text;
}

//**** END UTILITY FUNCTIONS ****//

function writetxt(text){

if(isloaded){
if(text!=0){
ishover=true;
if(NS4)text='<div class="navtext">'+((centertext)?'<center>':'')+text+((centertext)?'</center>':'')+'</div>';
writeindiv(text);
getboxheight();
if((W3C || IE4) && dofade){
ieop=0;
incropacity();
}}
else{
	if(NS4)navtxt.visibility="hide";
	if(IE4||W3C){
		if(dofade)clearTimeout(op_id);
		navtxt.style.visibility="hidden";
	}
	writeindiv('');
	ishover=false;
	
	}
}}

function incropacity(){
if(ieop<=100){
ieop+=10;
if(IE4 || IE5)navtxt.style.filter="alpha(opacity="+ieop+")";
if(NS6)navtxt.style.MozOpacity=ieop/100;
op_id=setTimeout('incropacity()', 50);
}}

function moveobj(evt){
if(isloaded && ishover){
margin=(IE4||IE5)? 1 : 23;
if(NS6)if(document.height+27-window.innerHeight<0)margin=15;
if(NS4)if(document.height-window.innerHeight<0)margin=10;
if (NS4){
mx=evt.pageX
my=evt.pageY
}
else if (NS6){
mx=evt.clientX
my=evt.clientY
}
else if (IE5){
mx=event.clientX
my=event.clientY
}
else if (IE4){
mx=0
my=0
}
else if (OPR){ //Lock add 2009.07.02 for Opera;
	mx=evt.clientX;
	my=evt.clientY;
}

if(NS4){
mx-=getpagescrollx();
my-=getpagescrolly();
}
xoff=(center)? mx-boxwidth/2 : mx+5;
yoff=(my+boxheight+10-getpagescrolly()+margin>=w_y)? -15-boxheight: 10; //10px lock modified.

//alert("w_x: "+ w_x + "\nboxwidth: "+ boxwidth + "\nxoff: "+xoff + "\nmargin: "+ margin + "\nyoff: "+ yoff);

movenavtxt( Math.min(w_x-boxwidth-margin , Math.max(2,xoff))+getpagescrollx() , my+yoff+getpagescrolly());
if(NS4)navtxt.visibility="show";
if(W3C||IE4)navtxt.style.visibility="visible";
}}

if(NS4)document.captureEvents(Event.MOUSEMOVE);
document.onmousemove=moveobj;
function load_alttxt_enviroment(){
  navtxt=(NS4)? document.layers['navtxt'] : (IE4)? document.all['navtxt'] : (W3C)? document.getElementById('navtxt') : null;
  getboxwidth();
  getboxheight();
  getwindowdims();    
  isloaded=true;
  if((W3C || IE4) && centertext)navtxt.style.textAlign="center";
  if(W3C)navtxt.style.padding='4px';
  if(IE4 || IE5 && dofade)navtxt.style.filter="alpha(opacity=0)";
}
window.onload=function(){
		load_alttxt_enviroment();
}
window.onresize=getwindowdims;