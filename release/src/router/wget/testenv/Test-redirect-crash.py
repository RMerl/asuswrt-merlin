#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile
import os

# This test caused wget up to 1.16 to crash
#os.environ["LC_ALL"] = "en_US.UTF-8"

urls = [
    "File%20formats/Images/SVG,%20Scalable%20Vector%20Graphics/html,%20W3C%20v1.2%20rec%20(tiny)/directory",
    "File formats/Images/SVG, Scalable Vector Graphics/html, W3C v1.2 rec (tiny)/directory/",
    "File%20formats/Images/SVG,%20Scalable%20Vector%20Graphics/html,%20W3C%20v1.2%20rec%20(tiny)/directory/somefile.rng",
    "File%20formats/Images/SVG%2C%20Scalable%20Vector%20Graphics/html%2c%20W3C%20v1.2%20rec%20%28tiny%29/directory/somefile.rng",
    "File%20formats/Images/SVG%2C%20Scalable%20Vector%20Graphics/html%2c%20W3C%20v1.2%20rec%20%28tiny%29/directory/",
    "File%20formats/Images/SVG%2C%20Scalable%20Vector%20Graphics/html%2C%20W3C%20v1.2%20rec%20%28tiny%29/directory"]


redirected = [
        "File%20formats/Images/SVG,%20Scalable%20Vector%20Graphics/html,%20W3C%20v1.2%20rec%20(tiny)/directory/"
]

############# File Definitions ###############################################
Index = ""
for i in urls:
    Index = Index + "<a href='/%s'></a>" % i

File1 = ""

def get_redirect(url):
    data = {
        "File%20formats/Images/SVG,%20Scalable%20Vector%20Graphics/html,%20W3C%20v1.2%20rec%20(tiny)/directory" :
           "File%20formats/Images/SVG,%20Scalable%20Vector%20Graphics/html,%20W3C%20v1.2%20rec%20(tiny)/directory/",
        "File%20formats/Images/SVG%2C%20Scalable%20Vector%20Graphics/html%2C%20W3C%20v1.2%20rec%20%28tiny%29/directory" :
           "File%20formats/Images/SVG,%20Scalable%20Vector%20Graphics/html,%20W3C%20v1.2%20rec%20(tiny)/directory/"
    }
    dest = data.get(url)
    if dest:
        return {"Response"          : 301,
                "SendHeader" : {"Location" : "/%s" % dest}}
    return None


index_url = "File%20formats/Images/SVG,%20Scalable%20Vector%20Graphics/html,%20W3C%20v1.2%20rec%20(tiny)/index.html"
Index_File = WgetFile (index_url, Index)
Files = ([Index_File] + [WgetFile(i, File1, rules=get_redirect(i)) for i in (redirected + urls)])

WGET_OPTIONS = "--recursive -e robots=off"

WGET_URLS = [[index_url]]

ExpectedReturnCode = 0

################ Pre and Post Test Hooks #####################################
pre_test = {
    "ServerFiles"       : [Files]
}
test_options = {
    "WgetCommands"      : WGET_OPTIONS,
    "Urls"              : WGET_URLS
}
post_test = {
    "ExpectedRetcode"   : ExpectedReturnCode,
}

err = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
).begin ()

exit (err)
