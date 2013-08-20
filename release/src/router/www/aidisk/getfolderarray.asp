<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">

<script>
var result = [<% get_folder_tree(); %>];
var layer_order = '<% get_parameter("layer_order"); %>';
var motion = '<% get_parameter("motion"); %>';

function get_folder_tree_success(){
	parent.get_tree_items(result, motion);
}
</script>
</head>

<body onload="get_folder_tree_success();">
</body>
</html>
