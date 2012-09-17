<?php

#ob_start(/*"ob_gzhandler"*/);
print "12345<br />\n";
#phpinfo();
#header("Content-Length: ".ob_get_length());
#ob_end_flush();

?>
