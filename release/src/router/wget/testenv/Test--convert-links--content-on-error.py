#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test ensures that Wget link conversion works also on HTTP error pages.
"""
############# File Definitions ###############################################
a_x_FileContent = """
<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8">
	<title></title>
</head>
<body>
	<a href="/b/y.html"></a>
</body>
</html>
"""
a_x_LocalFileContent = """
<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="UTF-8">
	<title></title>
</head>
<body>
	<a href="../b/y.html"></a>
</body>
</html>
"""

error_FileContent = '404 page content'
error_FileRules = {
    'Response': 404 ,
    'SendHeader': {
        'Content-Length': len(error_FileContent),
        'Content-Type': 'text/plain',
    }
}

a_x_File = WgetFile ("a/x.html", a_x_FileContent)
robots_File = WgetFile ("robots.txt", '')
error_File = WgetFile ("b/y.html", error_FileContent, rules=error_FileRules)

B_File = WgetFile ("a/x.html", a_x_LocalFileContent)

WGET_OPTIONS = "--no-host-directories -r -l2 --convert-links --content-on-error"
WGET_URLS = [["a/x.html"]]

Files = [[a_x_File, robots_File, error_File]]

ExpectedReturnCode = 8
ExpectedDownloadedFiles = [B_File, robots_File, error_File]

################ Pre and Post Test Hooks #####################################
pre_test = {
    "ServerFiles"       : Files
}
test_options = {
    "WgetCommands"      : WGET_OPTIONS,
    "Urls"              : WGET_URLS
}
post_test = {
    "ExpectedFiles"     : ExpectedDownloadedFiles,
    "ExpectedRetcode"   : ExpectedReturnCode
}

err = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
