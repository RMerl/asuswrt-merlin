// Customization that will sort static IP table by clicking the column headers.
// This file is self-contained and can just be removed from the system in case it should become incompatible with future firmware updates.
// It only depends on the jQuery being available, the global variable "dhcp_staticlist_array", the global function showdhcp_staticlist(), and a <td> element with id=GWStatic - and of course that the HTML structure doesn't change too much.
// The only interference it has with the global scope is the variable "merlinWS"
// Contribution by Allan Jensen, WinterNet Studio, www.winternet.no. Anyone is free do use and do with the code whatever they want.
var merlinWS = {
	sortStaticIpList: function(colIndex) {
		var theList = merlinWS.parseAsusList(dhcp_staticlist_array);

		if (colIndex == 1) {
			theList.sort(merlinWS.sortBy(colIndex, false, null, {isIp: true}));
		} else {
			theList.sort(merlinWS.sortBy(colIndex, false));
		}
		dhcp_staticlist_array = merlinWS.buildAsusList(theList);
		showdhcp_staticlist();  //update display
	},


	// Utility functions
	parseAsusList: function(str) {
		var output = [], arr, row;
		if (str.length > 0) {
			arr = str.split('&#60');
			for (var i in arr) {
				if (typeof arr[i] == 'string' && arr[i].length > 0) {
					row = arr[i].split('&#62');
					output.push(row);
				}
			}
		}
		return output;
	},

	buildAsusList: function(arr) {
		var str = '';
		for (var i in arr) {
			if (typeof arr[i] == 'function') continue;
			str += '&#60';
			for (var j in arr[i]) {
				if (typeof arr[i][j] == 'function') continue;
				str += arr[i][j] +'&#62';
			}
			str = str.substr(str, str.length-4);  //remove last "&#62"
		}
		return str;
	},

	sortBy: function(field, reverse, primer, options) {
		// Sort a two-dimension array (an array where each entry is an object of which we can sort by a given key)
		var key = primer ? 
				 function(x) {
					 return primer((typeof x[field] == 'string' ? x[field].toUpperCase() : x[field]));  //convert to upper to make case insensitive
				 } : 
				 function(x) {
					 return (typeof x[field] == 'string' ? x[field].toUpperCase() : x[field]);  //convert to upper to make case insensitive
				 };

		reverse = !reverse ? 1 : -1;

		if (typeof options == 'object' && options.isIp) {
			// Sort a list of IP addresses
			return function (a, b) {
				aa = a[field].split(".");
				bb = b[field].split(".");
	
				var resulta = aa[0]*0x1000000 + aa[1]*0x10000 + aa[2]*0x100 + aa[3]*1;
				var resultb = bb[0]*0x1000000 + bb[1]*0x10000 + bb[2]*0x100 + bb[3]*1;


				if (reverse == -1) {
					return resultb-resulta;
				} else {
					return resulta-resultb;
				}
			}
		} else {
			// Sort any other type of list
			return function (a, b) {
				 return a = key(a), b = key(b), reverse * ((a > b) - (b > a));
			} 
		}
	}
};

jQuery(function($) {
	var staticHeaderTable = $('#GWStatic').closest('table');  //depends on a <td> with id=GWStatic

	if (staticHeaderTable.length > 0) {  //only if we find the table
		$(staticHeaderTable).find('th').slice(0,3).css({cursor: 'pointer'});

		// NOTE: if using borderTop Chrome has a strange bug that due to the colspan above makes it display incorrectly!
		$(staticHeaderTable).find('th:eq(0)').on('click', function(e) {
			merlinWS.sortStaticIpList(0);
			$(staticHeaderTable).find('th').css({borderBottom: 'none'});
			$(e.target).closest('th').css({borderBottom: '2px solid #fc0'});
		});

		$(staticHeaderTable).find('th:eq(1)').on('click', function(e) {
			merlinWS.sortStaticIpList(1);
			$(staticHeaderTable).find('th').css({borderBottom: 'none'});
			$(e.target).closest('th').css({borderBottom: '2px solid #fc0'});
		});

		$(staticHeaderTable).find('th:eq(2)').on('click', function(e) {
			merlinWS.sortStaticIpList(2);
			$(staticHeaderTable).find('th').css({borderBottom: 'none'});
			$(e.target).closest('th').css({borderBottom: '2px solid #fc0'});
		});
	}
});
