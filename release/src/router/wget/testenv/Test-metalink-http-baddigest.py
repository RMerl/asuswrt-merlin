#!/usr/bin/env python3
from sys import exit
from test.http_test import HTTPTest
from misc.wget_file import WgetFile
import hashlib
from base64 import b64encode

"""
    This is to test Metalink/HTTP with a malformed base64 Digest header.

    With --trust-server-names, trust the metalink:file names.

    Without --trust-server-names, don't trust the metalink:file names:
    use the basename of --input-metalink, and add a sequential number
    (e.g. .#1, .#2, etc.).

    Strip the directory from unsafe paths.
"""

############# File Definitions ###############################################
bad = "Ouch!"
bad_sha256 = b64encode (hashlib.sha256 (bad.encode ('UTF-8')).digest ()).decode ('ascii')

LinkHeaders = ["<http://{{SRV_HOST}}:{{SRV_PORT}}/wrong_file>; rel=duplicate; pri=1"]
DigestHeader = "SHA-256=bad_base64,SHA-256={{BAD_HASH}}"

# This will be filled as soon as we know server hostname and port
MetaHTTPRules = {'SendHeader' : {}}

MetaHTTP = WgetFile ("main.metalink", rules=MetaHTTPRules)

wrong_file = WgetFile ("wrong_file", bad)
wrong_file_down = WgetFile ("main.metalink", bad)

WGET_OPTIONS = "--metalink-over-http"
WGET_URLS = [["main.metalink"]]

RequestList = [[
    "HEAD /main.metalink",
    "GET /wrong_file"
]]

Files = [[
    MetaHTTP,
    wrong_file
]]
Existing_Files = []

ExpectedReturnCode = 0
ExpectedDownloadedFiles = [wrong_file_down]

################ Pre and Post Test Hooks #####################################
pre_test = {
    "ServerFiles"       : Files,
    "LocalFiles"        : Existing_Files
}
test_options = {
    "WgetCommands"      : WGET_OPTIONS,
    "Urls"              : WGET_URLS
}
post_test = {
    "ExpectedFiles"     : ExpectedDownloadedFiles,
    "ExpectedRetcode"   : ExpectedReturnCode,
    "FilesCrawled"      : RequestList
}

http_test = HTTPTest (
                pre_hook=pre_test,
                test_params=test_options,
                post_hook=post_test
)

http_test.server_setup()
### Get and use dynamic server sockname
srv_host, srv_port = http_test.servers[0].server_inst.socket.getsockname ()

DigestHeader = DigestHeader.replace('{{BAD_HASH}}', bad_sha256)

# Helper function for hostname, port and digest substitution
def SubstituteServerInfo (text, host, port):
    text = text.replace('{{SRV_HOST}}', host)
    text = text.replace('{{SRV_PORT}}', str (port))
    return text

MetaHTTPRules["SendHeader"] = {
        'Link': [ SubstituteServerInfo (LinkHeader, srv_host, srv_port)
                    for LinkHeader in LinkHeaders ],
        'Digest': DigestHeader
}

err = http_test.begin ()

exit (err)
