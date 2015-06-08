<html>
<head>
<style>
.string{
	font-family: Arial, Helvetica, sans-serif;
	color:#118888;
	font-size:24px;
}

.hint{
	font-family: Arial, Helvetica, sans-serif;
	color:#aa0000;
	font-size:18px;
}
</style>

<script>
var apps_array = <% apps_info("asus"); %>;
</script>
</head>

<body>
<h2>Please view the source code of this page to see the array of the installed APPs!</h2>
<form name="form" method="post" action="/apps_test.asp">
<span class="string">apps_action: <input type="text" name="apps_action" value="" maxlength="32" autocorrect="off" autocapitalize="off"></span>
<span class="hint">(ex: install, update, upgrade, remove, enable, switch)</span><br>
<span class="string">apps_name: <input type="text" name="apps_name" value="" maxlength="32" autocorrect="off" autocapitalize="off"></span><br>
<span class="string">apps_flag: <input type="text" name="apps_flag" value="" maxlength="32" autocorrect="off" autocapitalize="off"></span><br>
<input type="submit">
</form>

<% apps_action(); %>

</body>
</html>
