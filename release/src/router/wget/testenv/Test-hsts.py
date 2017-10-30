#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from test.base_test import HTTP, HTTPS
from misc.wget_file import WgetFile
import time
import os

"""
This test makes sure Wget can parse a given HSTS database and apply the indicated HSTS policy.
"""

print (os.getenv('SSL_TESTS'))
if os.getenv('SSL_TESTS') is None:
    exit (77)

def hsts_database_path():
    hsts_file = ".wget-hsts-testenv"
    return os.path.abspath(hsts_file)

def create_hsts_database(path, host, port):
    # we want the current time as an integer,
    # not as a floating point
    curtime = int(time.time())
    max_age = "123456"

    f = open(path, "w")

    f.write("# dummy comment\n")
    f.write(host + "\t" + str(port) + "\t0\t" + str(curtime) + "\t" + max_age + "\n")
    f.close()

File_Name = "hw"
File_Content = "Hello, world!"
File = WgetFile(File_Name, File_Content)

Hsts_File_Path = hsts_database_path()

CAFILE = os.path.abspath(os.path.join(os.getenv('srcdir', '.'), 'certs', 'ca-cert.pem'))

WGET_OPTIONS = "--hsts-file=" + Hsts_File_Path + " --ca-certificate=" + CAFILE
WGET_URLS = [[File_Name]]

Files = [[File]]
Servers = [HTTPS]
Requests = ["http"]

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [File]

pre_test = {
        "ServerFiles"   : Files,
        "Domains"       : ["localhost"]
}
post_test = {
        "ExpectedFiles"     : ExpectedDownloadedFiles,
        "ExpectedRetCode"   : ExpectedReturnCode,
}
test_options = {
        "WgetCommands"  : WGET_OPTIONS,
        "Urls"          : WGET_URLS
}

test = HTTPTest(
        pre_hook = pre_test,
        post_hook = post_test,
        test_params = test_options,
        protocols = Servers,
        req_protocols = Requests
)

# start the web server and create the temporary HSTS database
test.setup()
create_hsts_database(Hsts_File_Path, 'localhost', test.port)

err = test.begin()

# remove the temporary HSTS database
os.unlink(hsts_database_path())
exit(err)
