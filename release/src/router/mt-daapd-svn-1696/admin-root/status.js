Event.observe(window,'load',initStatus);

var UPDATE_FREQUENCY = 5000; // number of milliseconds between page updates

function initStatus(e) {
  Updater.update();
  // Bind events to the buttons
  Event.observe('button_stop_server','click',Updater.stopServer);
  Event.observe('button_start_scan','click',Updater.startScan);
  Event.observe('button_start_full_scan','click',Updater.startFullScan);
}
var Updater = {
  stop: false,
  wait: function () {
    Updater.updateTimeout = window.setTimeout(Updater.update,UPDATE_FREQUENCY);
    Updater.spinnerTimeout = window.setTimeout(Util.startSpinner,UPDATE_FREQUENCY-1000);
  },
  update: function () {
    if (Updater.updateTimeout) {
      window.clearTimeout(Updater.updateTimeout);
    }
    if (Updater.spinnerTimeout) {
      window.clearTimeout(Updater.spinnerTimeout);
    }
    if (Updater.stop) {
      return;
    }
    new Ajax.Request('xml-rpc?method=stats',{method: 'get',onComplete: Updater.rsStats});          
  },
  rsStats: function(request) {
    Util.stopSpinner();
    ['service','stat'].each(function (tag) {
      $A(request.responseXML.getElementsByTagName(tag)).each(function (element) {
        var node = $(Element.textContent(element.firstChild).toLowerCase().replace(/\ /,'_'));
        var status = Element.textContent(element.childNodes[1]);
        node.replaceChild(document.createTextNode(status),node.firstChild);
        if ('Idle' == status) {
          $('button_start_scan').disabled = false;
          $('button_start_full_scan').disabled = false;
        }
      });
    });  
    var thread = $A(request.responseXML.getElementsByTagName('thread'));
    var threadTable = new Table('thread');
    threadTable.removeTBodyRows();
    var users = 0;
    thread.each(function (element) {
      users++;
      row = [];
      row.push(Element.textContent(element.childNodes[1]));
      row.push(Element.textContent(element.childNodes[2]));
      threadTable.addTbodyRow(row);    
    });
 
    // Monkey see, monkey do
    var plugin = $A(request.responseXML.getElementsByTagName('plugin'));
    var pluginTable = new Table('plugin');
    pluginTable.removeTBodyRows();
    plugin.each(function(element) {
      row = [];
      info = Element.textContent(element.childNodes[0]).split('/',2);
      row.push(info[0]);
      row.push(info[1]);
      pluginTable.addTbodyRow(row);
    });

    // $('session_count').replaceChild(document.createTextNode(users + ' Connected Users'),$('session_count').firstChild);
    if (!Updater.stop) {
      Updater.wait();
    }
  },
  stopServer: function() {
    Util.stopSpinner();
    new Ajax.Request('xml-rpc',{method:'post',parameters: 'method=shutdown',onComplete: Updater.rsStopServer});
  },
  rsStopServer: function(request) {
    Updater.stop = true;
    Updater.update(); // To clear the spinner timeout
    Element.show('grey_screen');
    Effect.Appear('server_stopped_message');
  },
  startScan: function(e) {
    Event.element(e).disabled = true;
    new Ajax.Request('xml-rpc',{method:'post',parameters: 'method=rescan',onComplete: Updater.rsStartScan});  
  },
  rsStartScan: function(request) {
    Updater.update();    
  },
  startFullScan: function(e) {
    Event.element(e).disabled = true;
    new Ajax.Request('xml-rpc',{method:'post',parameters: 'method=rescan&full=1',onComplete: Updater.rsStartFullScan});    
  },
  rsStartFullScan: function(request) {
    Updater.update();
  }
}

Table = Class.create();
Object.extend(Table.prototype,{
  initialize: function (id) {
    this.tbody = $(id).getElementsByTagName('tbody')[0];  
  },
  removeTBodyRows: function () {
    Element.removeChildren(this.tbody);
  },
  addTbodyRow: function (cells) {
    var tr = document.createElement('tr');
    cells.each(function (cell) {
      var td = document.createElement('td');
      td.appendChild(document.createTextNode(cell));
      tr.appendChild(td);
    });
    this.tbody.appendChild(tr);
  }
});