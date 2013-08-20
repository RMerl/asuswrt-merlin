<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">

<script>
var result = [<% get_share_tree(); %>];
var layer_order = '<% get_parameter("layer_order"); %>';
var motion = '<% get_parameter("motion"); %>';

function get_share_tree_success(){
	/*result = [];
	if(layer_order.length == 1)
		result = ["Upper USB#0#2", "Lower USB#1#1"];
	else if(layer_order == "0_0")
		result = ["part0#0#1", "part1#1#1"];
	else if(layer_order == "0_1")
		result = ["part2#0#1", "part3#1#1", "part4#2#1"];
	else
		result = ["share#0#0"];//*/
	
	/*result = [];
	if(layer_order.length == 1)
		/*result = ["Upper USB#0#2", "Lower USB#1#0"];
	else if(layer_order == "0_0")
		result = ["part0#0#1", "part1#1#1"];
	else
		result = ["share#0#0"];//*/
	
	parent.get_tree_items(result, motion);
}
</script>
</head>

<body onload="get_share_tree_success();">
</body>
</html>
