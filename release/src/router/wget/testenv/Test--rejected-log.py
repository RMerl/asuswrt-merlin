#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile

"""
    This test executed Wget in recursive mode with a rejected log outputted.
"""
############# File Definitions ###############################################
mainpage = """
<html>
<head>
  <title>Main Page</title>
</head>
<body>
  <p>
    Recurse to a <a href="http://localhost:{{port}}/secondpage.html">second page</a>.
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
    Recurse to a <a href="http://localhost:{{port}}/thirdpage.html">third page</a>.
    Try the blacklisted <a href="http://localhost:{{port}}/index.html">main page</a>.
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
    Try a hidden <a href="http://localhost:{{port}}/dummy.txt">dummy file</a>.
    Try to leave to <a href="http://no.such.domain/">another domain</a>.
  </p>
</body>
</html>
"""

robots = """
User-agent: *
Disallow: /dummy.txt
"""

log = """\
REASON\tU_URL\tU_SCHEME\tU_HOST\tU_PORT\tU_PATH\tU_PARAMS\tU_QUERY\tU_FRAGMENT\tP_URL\tP_SCHEME\tP_HOST\tP_PORT\tP_PATH\tP_PARAMS\tP_QUERY\tP_FRAGMENT
BLACKLIST\thttp%3A//localhost%3A{{port}}/index.html\tSCHEME_HTTP\tlocalhost\t{{port}}\tindex.html\t\t\t\thttp%3A//localhost%3A{{port}}/secondpage.html\tSCHEME_HTTP\tlocalhost\t{{port}}\tsecondpage.html\t\t\t
ROBOTS\thttp%3A//localhost%3A{{port}}/dummy.txt\tSCHEME_HTTP\tlocalhost\t{{port}}\tdummy.txt\t\t\t\thttp%3A//localhost%3A{{port}}/thirdpage.html\tSCHEME_HTTP\tlocalhost\t{{port}}\tthirdpage.html\t\t\t
SPANNEDHOST\thttp%3A//no.such.domain/\tSCHEME_HTTP\tno.such.domain\t80\t\t\t\t\thttp%3A//localhost%3A{{port}}/thirdpage.html\tSCHEME_HTTP\tlocalhost\t{{port}}\tthirdpage.html\t\t\t
"""

dummyfile = "Don't care."


index_html = WgetFile ("index.html", mainpage)
secondpage_html = WgetFile ("secondpage.html", secondpage)
thirdpage_html = WgetFile ("thirdpage.html", thirdpage)
robots_txt = WgetFile ("robots.txt", robots)
dummy_txt = WgetFile ("dummy.txt", dummyfile)
log_csv = WgetFile ("log.csv", log)

WGET_OPTIONS = "-nd -r --rejected-log log.csv"
WGET_URLS = [["index.html"]]

Files = [[index_html, secondpage_html, thirdpage_html, robots_txt, dummy_txt]]

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [index_html, secondpage_html, thirdpage_html, robots_txt, log_csv]

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
