#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test executed Wget in Spider mode with recursive retrieval.
"""
############# File Definitions ###############################################
mainpage = """
<html>
<head>
  <title>Main Page</title>
</head>
<body>
  <p>
    Some text and a link to a <a href="http://localhost:{{port}}/secondpage.html">second page</a>.
    Also, a <a href="http://localhost:{{port}}/nonexistent">broken link</a>.
  </p>
</body>
</html>
"""


secondpage = """
<html>
<head>
  <title>Second Page</title>
</head>
<body>
  <p>
    Some text and a link to a <a href="http://localhost:{{port}}/thirdpage.html">third page</a>.
    Also, a <a href="http://localhost:{{port}}/nonexistent">broken link</a>.
  </p>
</body>
</html>
"""

thirdpage = """
<html>
<head>
  <title>Third Page</title>
</head>
<body>
  <p>
    Some text and a link to a <a href="http://localhost:{{port}}/dummy.txt">text file</a>.
    Also, another <a href="http://localhost:{{port}}/againnonexistent">broken link</a>.
  </p>
</body>
</html>
"""

dummyfile = "Don't care."


index_html = WgetFile ("index.html", mainpage)
secondpage_html = WgetFile ("secondpage.html", secondpage)
thirdpage_html = WgetFile ("thirdpage.html", thirdpage)
dummy_txt = WgetFile ("dummy.txt", dummyfile)

Request_List = [
    [
        "HEAD /",
        "GET /",
        "GET /robots.txt",
        "HEAD /secondpage.html",
        "GET /secondpage.html",
        "HEAD /nonexistent",
        "HEAD /thirdpage.html",
        "GET /thirdpage.html",
        "HEAD /dummy.txt",
        "HEAD /againnonexistent"
    ]
]

WGET_OPTIONS = "--spider -r"
WGET_URLS = [[""]]

Files = [[index_html, secondpage_html, thirdpage_html, dummy_txt]]

ExpectedReturnCode = 8
ExpectedDownloadedFiles = []

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
    "ExpectedRetcode"   : ExpectedReturnCode,
    "FilesCrawled"      : Request_List
}

err = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
