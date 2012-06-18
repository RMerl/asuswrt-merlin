/* 
Correctly handle PNG transparency in Win IE 5.5 & 6.
Copyright 2007 Ignia, LLC
Based in part on code from from http://homepage.ntlworld.com/bobosola.

Use in <HEAD> with DEFER keyword wrapped in conditional comments:
<!--[if lt IE 7]>
<script defer type="text/javascript" src="pngfix.js"></script>
<![endif]-->

*/

function fixPng() {
  var arVersion = navigator.appVersion.split("MSIE")
  var version = parseFloat(arVersion[1])

  if ((version >= 5.5 && version < 7.0) && (document.body.filters)) {
    for(var i=0; i<document.images.length; i++) {
      var img = document.images[i];
      var imgName = img.src.toUpperCase();
      if (imgName.indexOf(".png") > 0) {
        var width = img.width;
        var height = img.height;
        var sizingMethod = (img.className.toLowerCase().indexOf("scale") >= 0)? "scale" : "image"; 
        img.runtimeStyle.filter = "progid:DXImageTransform.Microsoft.AlphaImageLoader(src='" + img.src.replace('%23', '%2523').replace("'", "%27") + "', sizingMethod='" + sizingMethod + "')";
        img.src = "images/blank.gif";
        img.width = width;
        img.height = height;
        }
      }
    }
  }

fixPng();

