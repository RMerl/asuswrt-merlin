/* JavaScript file containing basic stuff */

var reqStatus;
var globalStatus = new Array();

function popUp(URL) 
{ day = new Date();
  id = day.getTime();
  eval("pagemtdaapdplPop = window.open(URL, 'mtdaapdple', 'toolbar=0,scrollbars=1,location=0,statusbar=0,menubar=0,resizable=1,width=500,height=240,left=320,top=448');");
}

/* Temporary stuff follows */

function processStatus() 
{ globalStatus['statistics'] = new Array();
  var xmldoc = reqStatus.responseXML;
}


function reqStatusStateChange()
{ if(reqStatus.readyState == 4) 
  { if(reqStatus.status == 200) 
    { processStatus();
    }
  }
}

function getStatusXML(url, async) 
{ // branch for native XMLHttpRequest object
  if (window.XMLHttpRequest) 
  { reqStatus = new XMLHttpRequest();
    reqStatus.onreadystatechange = reqStatusStateChange;
    reqStatus.open("GET", url, async);
    return reqStatus.send(null);
    // branch for IE/Windows ActiveX version
  } else if (window.ActiveXObject) 
  { reqStatus = new ActiveXObject("Microsoft.XMLHTTP");
    if (reqStatus) 
    { reqStatus.onreadystatechange = reqStatusStateChange;
      reqStatus.open("GET", url, async);
      return reqStatus.send();
    }
  }
}


