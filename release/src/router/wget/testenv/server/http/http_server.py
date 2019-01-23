from http.server import HTTPServer, BaseHTTPRequestHandler
from exc.server_error import ServerError, AuthError, NoBodyServerError
from socketserver import BaseServer
from posixpath import basename, splitext
from base64 import b64encode
from random import random
from hashlib import md5
import threading
import socket
import os


class StoppableHTTPServer(HTTPServer):
    """ This class extends the HTTPServer class from default http.server library
    in Python 3. The StoppableHTTPServer class is capable of starting an HTTP
    server that serves a virtual set of files made by the WgetFile class and
    has most of its properties configurable through the server_conf()
    method. """

    request_headers = list()

    """ Define methods for configuring the Server. """

    def server_conf(self, filelist, conf_dict):
        """ Set Server Rules and File System for this instance. """
        self.server_configs = conf_dict
        self.fileSys = filelist

    def get_req_headers(self):
        return self.request_headers


class HTTPSServer(StoppableHTTPServer):
    """ The HTTPSServer class extends the StoppableHTTPServer class with
    additional support for secure connections through SSL. """

    def __init__(self, address, handler):
        import ssl
        BaseServer.__init__(self, address, handler)
        # step one up because test suite change directory away from $srcdir
        # (don't do that !!!)
        CERTFILE = os.path.abspath(os.path.join('..',
                                                os.getenv('srcdir', '.'),
                                                'certs',
                                                'server-cert.pem'))
        KEYFILE = os.path.abspath(os.path.join('..',
                                               os.getenv('srcdir', '.'),
                                               'certs',
                                               'server-key.pem'))
        self.socket = ssl.wrap_socket(
            sock=socket.socket(self.address_family, self.socket_type),
            ssl_version=ssl.PROTOCOL_TLSv1,
            certfile=CERTFILE,
            keyfile=KEYFILE,
            server_side=True
        )
        self.server_bind()
        self.server_activate()


class _Handler(BaseHTTPRequestHandler):
    """ This is a private class which tells the server *HOW* to handle each
    request. For each HTTP Request Command that the server should be capable of
    responding to, there must exist a do_REQUESTNAME() method which details the
    steps in which such requests should be processed. The rest of the methods
    in this class are auxiliary methods created to help in processing certain
    requests. """

    def get_rule_list(self, name):
        return self.rules.get(name)

    # The defailt protocol version of the server we run is HTTP/1.1 not
    # HTTP/1.0 which is the default with the http.server module.
    protocol_version = 'HTTP/1.1'

    """ Define functions for various HTTP Requests. """

    def do_HEAD(self):
        self.send_head("HEAD")

    def do_GET(self):
        """ Process HTTP GET requests. This is the same as processing HEAD
        requests and then actually transmitting the data to the client. If
        send_head() does not specify any "start" offset, we send the complete
        data, else transmit only partial data. """

        content, start = self.send_head("GET")
        if content:
            if start is None:
                self.wfile.write(content.encode('utf-8'))
            else:
                self.wfile.write(content.encode('utf-8')[start:])

    def do_POST(self):
        """ According to RFC 7231 sec 4.3.3, if the resource requested in a POST
        request does not exist on the server, the first POST request should
        create that resource. PUT requests are otherwise used to create a
        resource. Hence, we call the handle for processing PUT requests if the
        resource requested does not already exist.

        Currently, when the server receives a POST request for a resource, we
        simply append the body data to the existing file and return the new
        file to the client. If the file does not exist, a new file is created
        using the contents of the request body. """

        path = self.path[1:]
        if path in self.server.fileSys:
            self.rules = self.server.server_configs.get(path)
            if not self.rules:
                self.rules = dict()

            if not self.custom_response():
                return(None, None)

            body_data = self.get_body_data()
            self.send_response(200)
            self.add_header("Content-type", "text/plain")
            content = self.server.fileSys.pop(path) + "\n" + body_data
            total_length = len(content)
            self.server.fileSys[path] = content
            self.add_header("Content-Length", total_length)
            self.add_header("Location", self.path)
            self.finish_headers()
            try:
                self.wfile.write(content.encode('utf-8'))
            except Exception:
                pass
        else:
            self.send_put(path)

    def do_PUT(self):
        path = self.path[1:]
        self.rules = self.server.server_configs.get(path)
        if not self.custom_response():
            return(None, None)
        self.send_put(path)

    """ End of HTTP Request Method Handlers. """

    """ Helper functions for the Handlers. """

    def parse_range_header(self, header_line, length):
        import re
        if header_line is None:
            return None
        if not header_line.startswith("bytes="):
            raise ServerError("Cannot parse header Range: %s" %
                              (header_line))
        regex = re.match(r"^bytes=(\d*)\-$", header_line)
        range_start = int(regex.group(1))
        if range_start >= length:
            raise ServerError("Range Overflow")
        return range_start

    def get_body_data(self):
        cLength_header = self.headers.get("Content-Length")
        cLength = int(cLength_header) if cLength_header is not None else 0
        body_data = self.rfile.read(cLength).decode('utf-8')
        return body_data

    def send_put(self, path):
        if path in self.server.fileSys:
            self.server.fileSys.pop(path, None)
            self.send_response(204)
        else:
            self.rules = dict()
            self.send_response(201)
        body_data = self.get_body_data()
        self.server.fileSys[path] = body_data
        self.add_header("Location", self.path)
        self.finish_headers()

    """ This empty method is called automatically when all the rules are
    processed for a given request. However, send_header() should only be called
    AFTER a response has been sent. But, at the moment of processing the rules,
    the appropriate response has not yet been identified. As a result, we defer
    the processing of this rule till later. Each do_* request handler MUST call
    finish_headers() instead of end_headers(). The finish_headers() method
    takes care of sending the appropriate headers before completing the
    response. """
    def SendHeader(self, header_obj):
        pass

    def send_cust_headers(self):
        header_obj = self.get_rule_list('SendHeader')
        if header_obj:
            for header in header_obj.headers:
                self.add_header(header, header_obj.headers[header])

    def finish_headers(self):
        self.send_cust_headers()
        try:
            for keyword, value in self._headers_dict.items():
                if isinstance(value, list):
                    for value_el in value:
                        self.send_header(keyword, value_el)
                else:
                    self.send_header(keyword, value)
            # Clear the dictionary of existing headers for the next request
            self._headers_dict.clear()
        except AttributeError:
            pass
        self.end_headers()

    def Response(self, resp_obj):
        self.send_response(resp_obj.response_code)
        if resp_obj.response_code == 304:
            raise NoBodyServerError("Conditional get falling to head")
        raise ServerError("Custom Response code sent.")

    def custom_response(self):
        codes = self.get_rule_list('Response')
        if codes:
            self.send_response(codes.response_code)
            self.finish_headers()
            return False
        else:
            return True

    def add_header(self, keyword, value):
        if not hasattr(self, "_headers_dict"):
            self._headers_dict = dict()
        self._headers_dict[keyword.lower()] = value

    def base64(self, data):
        string = b64encode(data.encode('utf-8'))
        return string.decode('utf-8')

    """ Send an authentication challenge.
    This method calls self.send_header() directly instead of using the
    add_header() method because sending multiple WWW-Authenticate headers
    actually makes sense and we do use that feature in some tests. """
    def send_challenge(self, auth_type, auth_parm):
        auth_type = auth_type.lower()
        if auth_type == "both":
            self.send_challenge("basic", auth_parm)
            self.send_challenge("digest", auth_parm)
            return
        if auth_type == "basic":
            challenge_str = 'BasIc realm="Wget-Test"'
        elif auth_type == "digest" or auth_type == "both_inline":
            self.nonce = md5(str(random()).encode('utf-8')).hexdigest()
            self.opaque = md5(str(random()).encode('utf-8')).hexdigest()
            # 'DIgest' to provoke a Wget failure with turkish locales
            challenge_str = 'DIgest realm="Test", nonce="%s", opaque="%s"' % (
                            self.nonce,
                            self.opaque)
            try:
                if auth_parm['qop']:
                    challenge_str += ', qop="%s"' % auth_parm['qop']
            except KeyError:
                pass
            if auth_type == "both_inline":
                # 'BasIc' to provoke a Wget failure with turkish locales
                challenge_str = 'BasIc realm="Wget-Test", ' + challenge_str
        self.send_header("WWW-Authenticate", challenge_str)

    def authorize_basic(self, auth_header, auth_rule):
        if auth_header is None or auth_header.split(' ')[0].lower() != 'basic':
            return False
        else:
            self.user = auth_rule.auth_user
            self.passw = auth_rule.auth_pass
            auth_str = "basic " + self.base64(self.user + ":" + self.passw)
            return True if auth_str.lower() == auth_header.lower() else False

    def parse_auth_header(self, auth_header):
        n = len("digest ")
        auth_header = auth_header[n:].strip()
        items = auth_header.split(", ")
        keyvals = [i.split("=", 1) for i in items]
        keyvals = [(k.strip(), v.strip().replace('"', '')) for k, v in keyvals]
        return dict(keyvals)

    def KD(self, secret, data):
        return self.H(secret + ":" + data)

    def H(self, data):
        return md5(data.encode('utf-8')).hexdigest()

    def A1(self):
        return "%s:%s:%s" % (self.user, "Test", self.passw)

    def A2(self, params):
        return "%s:%s" % (self.command, params["uri"])

    def check_response(self, params):
        if "qop" in params:
            data_str = params['nonce'] \
               + ":" + params['nc'] \
               + ":" + params['cnonce'] \
               + ":" + params['qop'] \
               + ":" + self.H(self.A2(params))
        else:
            data_str = params['nonce'] + ":" + self.H(self.A2(params))
        resp = self.KD(self.H(self.A1()), data_str)

        return True if resp == params['response'] else False

    def authorize_digest(self, auth_header, auth_rule):
        if auth_header is None or \
           auth_header.split(' ')[0].lower() != 'digest':
            return False
        else:
            self.user = auth_rule.auth_user
            self.passw = auth_rule.auth_pass
            params = self.parse_auth_header(auth_header)
            if self.user != params['username'] or \
                    self.nonce != params['nonce'] or \
                    self.opaque != params['opaque']:
                return False
            req_attribs = ['username', 'realm', 'nonce', 'uri', 'response']
            for attrib in req_attribs:
                if attrib not in params:
                    return False
            if not self.check_response(params):
                return False

    def authorize_both(self, auth_header, auth_rule):
        return False

    def authorize_both_inline(self, auth_header, auth_rule):
        return False

    def Authentication(self, auth_rule):
        try:
            self.handle_auth(auth_rule)
        except AuthError as se:
            self.send_response(401, "Authorization Required")
            self.send_challenge(auth_rule.auth_type, auth_rule.auth_parm)
            raise se

    def handle_auth(self, auth_rule):
        is_auth = True
        auth_header = self.headers.get("Authorization")
        required_auth = auth_rule.auth_type.lower()
        if required_auth == "both" or required_auth == "both_inline":
            if auth_header:
                auth_type = auth_header.split(' ')[0].lower()
            else:
                auth_type = required_auth
        else:
            auth_type = required_auth
        try:
            assert hasattr(self, "authorize_" + auth_type)
            is_auth = getattr(self, "authorize_" + auth_type)(auth_header,
                                                              auth_rule)
        except AssertionError:
            raise AuthError("Authentication Mechanism %s not supported" %
                            auth_type)
        except AttributeError as ae:
            raise AuthError(ae.__str__())
        if is_auth is False:
            raise AuthError("Unable to Authenticate")

    def ExpectHeader(self, header_obj):
        exp_headers = header_obj.headers
        for header_line in exp_headers:
            header_recd = self.headers.get(header_line)
            if header_recd is None or header_recd != exp_headers[header_line]:
                self.send_error(400, "Expected Header %s not found" %
                                header_line)
                raise ServerError("Header " + header_line + " not found")

    def RejectHeader(self, header_obj):
        rej_headers = header_obj.headers
        for header_line in rej_headers:
            header_recd = self.headers.get(header_line)
            if header_recd and header_recd == rej_headers[header_line]:
                self.send_error(400, 'Blacklisted Header %s received' %
                                header_line)
                raise ServerError("Header " + header_line + ' received')

    def __log_request(self, method):
        req = method + " " + self.path
        self.server.request_headers.append(req)

    def send_head(self, method):
        """ Common code for GET and HEAD Commands.
        This method is overridden to use the fileSys dict.

        The method variable contains whether this was a HEAD or a GET Request.
        According to RFC 2616, the server should not differentiate between
        the two requests, however, we use it here for a specific test.
        """

        if self.path == "/":
            path = "index.html"
        else:
            path = self.path[1:]

        self.__log_request(method)

        if path in self.server.fileSys:
            self.rules = self.server.server_configs.get(path)

            content = self.server.fileSys.get(path)
            content_length = len(content)

            for rule_name in self.rules:
                try:
                    assert hasattr(self, rule_name)
                    getattr(self, rule_name)(self.rules[rule_name])
                except AssertionError as ae:
                    msg = "Rule " + rule_name + " not defined"
                    self.send_error(500, msg)
                    return(None, None)
                except AuthError as ae:
                    print(ae.__str__())
                    self.finish_headers()
                    return(None, None)
                except NoBodyServerError as nbse:
                    print(nbse.__str__())
                    self.finish_headers()
                    return(None, None)
                except ServerError as se:
                    print(se.__str__())
                    self.add_header("Content-Length", content_length)
                    self.finish_headers()
                    return(content, None)

            try:
                self.range_begin = self.parse_range_header(
                    self.headers.get("Range"), content_length)
            except ServerError as ae:
                # self.log_error("%s", ae.err_message)
                if ae.err_message == "Range Overflow":
                    self.send_response(416)
                    self.finish_headers()
                    return(None, None)
                else:
                    self.range_begin = None
            if self.range_begin is None:
                self.send_response(200)
            else:
                self.send_response(206)
                self.add_header("Accept-Ranges", "bytes")
                self.add_header("Content-Range",
                                "bytes %d-%d/%d" % (self.range_begin,
                                                    content_length - 1,
                                                    content_length))
                content_length -= self.range_begin
            cont_type = self.guess_type(path)
            self.add_header("Content-Type", cont_type)
            self.add_header("Content-Length", content_length)
            self.finish_headers()
            return(content, self.range_begin)
        else:
            self.send_error(404, "Not Found")
            return(None, None)

    def guess_type(self, path):
        base_name = basename("/" + path)
        name, ext = splitext(base_name)
        extension_map = {
            ".txt":    "text/plain",
            ".css":    "text/css",
            ".html":   "text/html"
        }
        return extension_map.get(ext, "text/plain")


class HTTPd(threading.Thread):
    server_class = StoppableHTTPServer
    handler = _Handler

    def __init__(self, addr=None):
        threading.Thread.__init__(self)
        if addr is None:
            addr = ('localhost', 0)
        self.server_inst = self.server_class(addr, self.handler)
        self.server_address = self.server_inst.socket.getsockname()[:2]

    def run(self):
        self.server_inst.serve_forever()

    def server_conf(self, file_list, server_rules):
        self.server_inst.server_conf(file_list, server_rules)


class HTTPSd(HTTPd):

    server_class = HTTPSServer

# vim: set ts=4 sts=4 sw=4 tw=79 et :
