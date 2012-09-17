<?php
	$env = $_GET["env"];
	print isset($_ENV[$env]) ? $_ENV[$env] : '';
?>
