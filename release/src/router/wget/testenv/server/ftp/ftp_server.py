import os
import re
import threading
import socket
import pyftpdlib.__main__
from pyftpdlib.ioloop import IOLoop
import pyftpdlib.handlers as Handle
from pyftpdlib.servers import FTPServer
from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib._compat import PY3, u, b, getcwdu, callable

class FTPDHandler (Handle.FTPHandler):

    def ftp_LIST (self, path):
        try:
            iterator = self.run_as_current_user(self.fs.get_list_dir, path)
        except (OSError, FilesystemError):
            err = sys.exc_info()[1]
            why = _strerror (err)
            self.respond ('550 %s. ' % why)
        else:
            if self.isRule ("Bad List") is True:
                iter_list = list ()
                for flist in iterator:
                    line = re.compile (r'(\s+)').split (flist.decode ('utf-8'))
                    line[8] = '0'
                    iter_l =  ''.join (line).encode ('utf-8')
                    iter_list.append (iter_l)
                iterator = (n for n in iter_list)
            producer = Handle.BufferedIteratorProducer (iterator)
            self.push_dtp_data (producer, isproducer=True, cmd="LIST")
            return path

    def ftp_PASV (self, line):
        if self._epsvall:
            self.respond ("501 PASV not allowed after EPSV ALL.")
            return
        self._make_epasv(extmode=False)
        if self.isRule ("FailPASV") is True:
            del self.server.global_rules["FailPASV"]
            self.socket.close ()

    def isRule (self, rule):
        rule_obj = self.server.global_rules[rule]
        return False if not rule_obj else rule_obj[0]

class FTPDServer (FTPServer):

    def set_global_rules (self, rules):
        self.global_rules = rules

class FTPd(threading.Thread):
    """A threaded FTP server used for running tests.

    This is basically a modified version of the FTPServer class which
    wraps the polling loop into a thread.

    The instance returned can be used to start(), stop() and
    eventually re-start() the server.
    """
    handler = FTPDHandler
    server_class = FTPDServer

    def __init__(self, addr=None):
        os.mkdir ('server')
        os.chdir ('server')
        try:
            HOST = socket.gethostbyname ('localhost')
        except socket.error:
            HOST = 'localhost'
        USER = 'user'
        PASSWD = '12345'
        HOME = getcwdu ()

        threading.Thread.__init__(self)
        self.__serving = False
        self.__stopped = False
        self.__lock = threading.Lock()
        self.__flag = threading.Event()
        if addr is None:
            addr = (HOST, 0)

        authorizer = DummyAuthorizer()
        authorizer.add_user(USER, PASSWD, HOME, perm='elradfmwM')  # full perms
        authorizer.add_anonymous(HOME)
        self.handler.authorizer = authorizer
        # lowering buffer sizes = more cycles to transfer data
        # = less false positive test failures
        self.handler.dtp_handler.ac_in_buffer_size = 32768
        self.handler.dtp_handler.ac_out_buffer_size = 32768
        self.server = self.server_class(addr, self.handler)
        self.host, self.port = self.server.socket.getsockname()[:2]
        os.chdir ('..')

    def set_global_rules (self, rules):
        self.server.set_global_rules (rules)

    def __repr__(self):
        status = [self.__class__.__module__ + "." + self.__class__.__name__]
        if self.__serving:
            status.append('active')
        else:
            status.append('inactive')
        status.append('%s:%s' % self.server.socket.getsockname()[:2])
        return '<%s at %#x>' % (' '.join(status), id(self))

    @property
    def running(self):
        return self.__serving

    def start(self, timeout=0.001):
        """Start serving until an explicit stop() request.
        Polls for shutdown every 'timeout' seconds.
        """
        if self.__serving:
            raise RuntimeError("Server already started")
        if self.__stopped:
            # ensure the server can be started again
            FTPd.__init__(self, self.server.socket.getsockname(), self.handler)
        self.__timeout = timeout
        threading.Thread.start(self)
        self.__flag.wait()

    def run(self):
        self.__serving = True
        self.__flag.set()
        while self.__serving:
            self.__lock.acquire()
            self.server.serve_forever(timeout=self.__timeout, blocking=False)
            self.__lock.release()
        self.server.close_all()

    def stop(self):
        """Stop serving (also disconnecting all currently connected
        clients) by telling the serve_forever() loop to stop and
        waits until it does.
        """
        if not self.__serving:
            raise RuntimeError("Server not started yet")
        self.__serving = False
        self.__stopped = True
        self.join()


def mk_file_sys (file_list):
    os.chdir ('server')
    for name, content in file_list.items ():
        file_h = open (name, 'w')
        file_h.write (content)
        file_h.close ()
    os.chdir ('..')

def filesys ():
    fileSys = dict ()
    os.chdir ('server')
    for parent, dirs, files in os.walk ('.'):
        for filename in files:
            file_handle = open (filename, 'r')
            file_content = file_handle.read ()
            fileSys[filename] = file_content
    os.chdir ('..')
    return fileSys
