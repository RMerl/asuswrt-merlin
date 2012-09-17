<?php

function pathencode($path) {
	return str_replace(',', '%2c', urlencode($path));
}

$val = "X-Sendfile2: " . pathencode(getcwd() . "/index.txt") . " " . $_GET["range"];

if ($_GET["range2"]) $val .= ", " . pathencode(getcwd() . "/index.txt") . " " . $_GET["range2"];

header($val);

?>