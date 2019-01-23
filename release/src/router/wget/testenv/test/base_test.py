import os
import shutil
import shlex
import traceback
import re
import time
import sys
from subprocess import call
from misc.colour_terminal import print_red, print_blue
from exc.test_failed import TestFailed
import conf

HTTP = "HTTP"
HTTPS = "HTTPS"


class BaseTest:

    """
    Class that defines methods common to both HTTP and FTP Tests.
    Note that this is an abstract class, subclasses must implement
        * stop_server()
        * instantiate_server_by(protocol)
    """

    def __init__(self, pre_hook, test_params, post_hook, protocols, req_protocols):
        """
        Define the class-wide variables (or attributes).
        Attributes should not be defined outside __init__.
        """
        self.name = os.path.basename(os.path.realpath(sys.argv[0]))
        # if pre_hook == None, then {} (an empty dict object) is passed to
        # self.pre_configs
        self.pre_configs = pre_hook or {}

        self.test_params = test_params or {}
        self.post_configs = post_hook or {}
        self.protocols = protocols

        if req_protocols is None:
            self.req_protocols = map(lambda p: p.lower(), self.protocols)
        else:
            self.req_protocols = req_protocols

        self.servers = []
        self.domains = []
        self.ports = []

        self.addr = None
        self.port = -1

        self.wget_options = ''
        self.urls = []

        self.tests_passed = True
        self.ready = False
        self.init_test_env()

        self.ret_code = 0

    def get_test_dir(self):
        return self.name + '-test'

    def init_test_env(self):
        test_dir = self.get_test_dir()
        try:
            os.mkdir(test_dir)
        except FileExistsError:
            shutil.rmtree(test_dir)
            os.mkdir(test_dir)
        os.chdir(test_dir)

    def get_domain_addr(self, addr):
        # TODO if there's a multiple number of ports, wouldn't it be
        # overridden to the port of the last invocation?
        # Set the instance variables 'addr' and 'port' so that
        # they can be queried by test cases.
        self.addr = str(addr[0])
        self.port = str(addr[1])

        return [self.addr, self.port]

    def server_setup(self):
        print_blue("Running Test %s" % self.name)
        for protocol in self.protocols:
            instance = self.instantiate_server_by(protocol)
            self.servers.append(instance)

            # servers instantiated by different protocols may differ in
            # ports and etc.
            # so we should record different domains respect to servers.
            domain = self.get_domain_addr(instance.server_address)
            self.domains.append('localhost')
            self.ports.append(domain[1])

    def exec_wget(self):
        cmd_line = self.gen_cmd_line()
        params = shlex.split(cmd_line)
        print(params)

        if os.getenv("SERVER_WAIT"):
            time.sleep(float(os.getenv("SERVER_WAIT")))

        try:
            ret_code = call(params, env={"HOME": os.getcwd()})
        except FileNotFoundError:
            raise TestFailed("The Wget Executable does not exist at the "
                             "expected path.")

        return ret_code

    def gen_cmd_line(self):
        test_path = os.path.abspath(".")
        wget_path = os.path.abspath(os.path.join(test_path,
                                                 "..", '..', 'src', "wget"))
        wget_options = '--debug --no-config %s' % self.wget_options

        valgrind = os.getenv("VALGRIND_TESTS", "")
        gdb = os.getenv("GDB_TESTS", "")

        # GDB has precedence over Valgrind
        # If both VALGRIND_TESTS and GDB_TESTS are defined,
        # GDB will be executed.
        if gdb == "1":
            cmd_line = 'gdb --args %s %s ' % (wget_path, wget_options)
        elif valgrind == "1":
            cmd_line = 'valgrind --error-exitcode=301 ' \
                                '--leak-check=yes ' \
                                '--track-origins=yes ' \
                                '--suppressions=../valgrind-suppression-ssl ' \
                                '%s %s ' % (wget_path, wget_options)
        elif valgrind not in ("", "0"):
            cmd_line = '%s %s %s ' % (os.getenv("VALGRIND_TESTS", ""),
                                      wget_path,
                                      wget_options)
        else:
            cmd_line = '%s %s ' % (wget_path, wget_options)

        for req_protocol, urls, domain, port in zip(self.req_protocols,
                                                    self.urls,
                                                    self.domains,
                                                    self.ports):
            # zip is function for iterating multiple lists at the same time.
            # e.g. for item1, item2 in zip([1, 5, 3],
            #                              ['a', 'e', 'c']):
            #          print(item1, item2)
            # generates the following output:
            # 1 a
            # 5 e
            # 3 c
            for url in urls:
                cmd_line += '%s://%s:%s/%s ' % (req_protocol, domain, port, url)


        print(cmd_line)

        return cmd_line

    def __test_cleanup(self):
        os.chdir('..')
        try:
            if not os.getenv("NO_CLEANUP"):
                shutil.rmtree(self.get_test_dir())
        except:
            print("Unknown Exception while trying to remove Test Environment.")
            self.tests_passed = False

    def _exit_test(self):
        self.__test_cleanup()

    def begin(self):
        return 0 if self.tests_passed else 100

    def call_test(self):
        self.hook_call(self.test_params, 'Test Option')

        try:
            self.ret_code = self.exec_wget()
        except TestFailed as e:
            raise e
        finally:
            self.stop_server()

    def do_test(self):
        self.pre_hook_call()
        self.call_test()
        self.post_hook_call()

    def hook_call(self, configs, name):
        for conf_name, conf_arg in configs.items():
            try:
                # conf.find_conf(conf_name) returns the required conf class,
                # then the class is instantiated with conf_arg, then the
                # conf instance is called with this test instance itself to
                # invoke the desired hook
                conf.find_conf(conf_name)(conf_arg)(self)
            except AttributeError:
                self.stop_server()
                raise TestFailed("%s %s not defined." %
                                 (name, conf_name))

    def pre_hook_call(self):
        self.hook_call(self.pre_configs, 'Pre Test Function')

    def post_hook_call(self):
        self.hook_call(self.post_configs, 'Post Test Function')

    def _replace_substring(self, string):
        """
        Replace first occurrence of "{{name}}" in @string with
        "getattr(self, name)".
        """
        pattern = re.compile(r'\{\{\w+\}\}')
        match_obj = pattern.search(string)
        if match_obj is not None:
            rep = match_obj.group()
            temp = getattr(self, rep.strip('{}'))
            string = string.replace(rep, temp)
        return string

    def instantiate_server_by(self, protocol):
        """
        Subclasses must override this method to actually instantiate servers
        for test cases.
        """
        raise NotImplementedError

    def stop_server(self):
        """
        Subclasses must implement this method in order to stop certain
        servers of different types.
        """
        raise NotImplementedError

    @staticmethod
    def get_server_rules(file_obj):
        """
        The handling of expect header could be made much better when the
        options are parsed in a true and better fashion. For an example,
        see the commented portion in Test-basic-auth.py.
        """
        server_rules = {}
        for rule_name, rule in file_obj.rules.items():
            server_rules[rule_name] = conf.find_conf(rule_name)(rule)
        return server_rules

    def __enter__(self):
        """
        Initialization for with statement.
        """
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """
        If the with statement got executed with no exception raised, then
        exc_type, exc_val, exc_tb are all None.
        """
        if exc_val:
            self.tests_passed = False
            if exc_type is TestFailed:
                print_red('Error: %s.' % exc_val.error)
            else:
                print_red('Unhandled exception caught.')
                print(exc_val)
                traceback.print_tb(exc_tb)
        self.__test_cleanup()

        return self.tests_passed
