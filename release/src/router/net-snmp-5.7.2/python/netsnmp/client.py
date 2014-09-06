import client_intf
import string
import re
import types
from sys import stderr

# control verbosity of error output
verbose = 1

secLevelMap = { 'noAuthNoPriv':1, 'authNoPriv':2, 'authPriv':3 }

def _parse_session_args(kargs):
    sessArgs = {
        'Version':3,
        'DestHost':'localhost',
        'Community':'public',
        'Timeout':1000000,
        'Retries':3,
        'RemotePort':161,
        'LocalPort':0,
        'SecLevel':'noAuthNoPriv',
        'SecName':'initial',
        'PrivProto':'DEFAULT',
        'PrivPass':'',
        'AuthProto':'DEFAULT',
        'AuthPass':'',
        'ContextEngineId':'',
        'SecEngineId':'',
        'Context':'',
        'Engineboots':0,
        'Enginetime':0,
        'UseNumeric':0,
        'OurIdentity':'',
        'TheirIdentity':'',
        'TheirHostname':'',
        'TrustCert':''
        }
    keys = kargs.keys()
    for key in keys:
        if sessArgs.has_key(key):
            sessArgs[key] = kargs[key]
        else:
            print >>stderr, "ERROR: unknown key", key
    return sessArgs

def STR(obj):
    if obj != None:
        obj = str(obj)
    return obj
    

class Varbind(object):
    def __init__(self, tag=None, iid=None, val=None, type=None):
        self.tag = STR(tag)
        self.iid = STR(iid)
        self.val = STR(val)
        self.type = STR(type)
        # parse iid out of tag if needed
        if iid == None and tag != None:
            regex = re.compile(r'^((?:\.\d+)+|(?:\w+(?:[-:]*\w+)+))\.?(.*)$')
            match = regex.match(tag)
            if match:
                (self.tag, self.iid) = match.group(1,2)

    def __setattr__(self, name, val):
        self.__dict__[name] = STR(val)

    def print_str(self):
        return self.tag, self.iid, self.val, self.type


class VarList(object):
    def __init__(self, *vs):
        self.varbinds = []

        for var in vs:
            if isinstance(var, netsnmp.client.Varbind):
                self.varbinds.append(var)
            else:
                self.varbinds.append(Varbind(var))

    def __len__(self):
        return len(self.varbinds)
    
    def __getitem__(self, index):
        return self.varbinds[index]

    def __setitem__(self, index, val):
        if isinstance(val, netsnmp.client.Varbind):
            self.varbinds[index] = val
        else:
            raise TypeError

    def __iter__(self):
        return iter(self.varbinds)

    def __delitem__(self, index):
        del self.varbinds[index]

    def __repr__(self):
        return repr(self.varbinds)

    def __getslice__(self, i, j):
        return self.varbinds[i:j]

    def append(self, *vars):
         for var in vars:
            if isinstance(var, netsnmp.client.Varbind):
                self.varbinds.append(var)
            else:
                raise TypeError
       


class Session(object):
    def __init__(self, **args):
        self.sess_ptr = None
        self.UseLongNames = 0
        self.UseNumeric = 0
        self.UseSprintValue = 0
        self.UseEnums = 0
        self.BestGuess = 0
        self.RetryNoSuch = 0
        self.ErrorStr = ''
        self.ErrorNum = 0
        self.ErrorInd = 0
    
        sess_args = _parse_session_args(args)

        for k,v in sess_args.items():
            self.__dict__[k] = v

            
        # check for transports that may be tunneled
        transportCheck = re.compile('^(tls|dtls|ssh)');
        match = transportCheck.match(sess_args['DestHost'])

        if match:
            self.sess_ptr = client_intf.session_tunneled(
                sess_args['Version'],
                sess_args['DestHost'],
                sess_args['LocalPort'],
                sess_args['Retries'],
                sess_args['Timeout'],
                sess_args['SecName'],
                secLevelMap[sess_args['SecLevel']],
                sess_args['ContextEngineId'],
                sess_args['Context'],
                sess_args['OurIdentity'],
                sess_args['TheirIdentity'],
                sess_args['TheirHostname'],
                sess_args['TrustCert'],
                );
        elif sess_args['Version'] == 3:
            self.sess_ptr = client_intf.session_v3(
                sess_args['Version'],
                sess_args['DestHost'],
                sess_args['LocalPort'],
                sess_args['Retries'],
                sess_args['Timeout'],
                sess_args['SecName'],
                secLevelMap[sess_args['SecLevel']],
                sess_args['SecEngineId'],
                sess_args['ContextEngineId'],
                sess_args['Context'],
                sess_args['AuthProto'],
                sess_args['AuthPass'],
                sess_args['PrivProto'],
                sess_args['PrivPass'],
                sess_args['Engineboots'],
                sess_args['Enginetime'])
        else:
            self.sess_ptr = client_intf.session(
                sess_args['Version'],
                sess_args['Community'],
                sess_args['DestHost'],
                sess_args['LocalPort'],
                sess_args['Retries'],
                sess_args['Timeout'])
        
    def get(self, varlist):
        res = client_intf.get(self, varlist)
        return res

    def set(self, varlist):
        res = client_intf.set(self, varlist)
        return res
    
    def getnext(self, varlist):
        res = client_intf.getnext(self, varlist)
        return res

    def getbulk(self, nonrepeaters, maxrepetitions, varlist):
        if self.Version == 1:
            return None
        res = client_intf.getbulk(self, nonrepeaters, maxrepetitions, varlist)
        return res

    def walk(self, varlist):
        res = client_intf.walk(self, varlist)
        return res

    def __del__(self):
        res = client_intf.delete_session(self)
        return res

import netsnmp
        
def snmpget(*args, **kargs):
    sess = Session(**kargs)
    var_list = VarList()
    for arg in args:
        if isinstance(arg, netsnmp.client.Varbind):
            var_list.append(arg)
        else:
            var_list.append(Varbind(arg))
    res = sess.get(var_list)
    return res

def snmpset(*args, **kargs):
    sess = Session(**kargs)
    var_list = VarList()
    for arg in args:
        if isinstance(arg, netsnmp.client.Varbind):
            var_list.append(arg)
        else:
            var_list.append(Varbind(arg))
    res = sess.set(var_list)
    return res

def snmpgetnext(*args, **kargs):
    sess = Session(**kargs)
    var_list = VarList()
    for arg in args:
        if isinstance(arg, netsnmp.client.Varbind):
            var_list.append(arg)
        else:
            var_list.append(Varbind(arg))
    res = sess.getnext(var_list)
    return res

def snmpgetbulk(nonrepeaters, maxrepetitions,*args, **kargs):
    sess = Session(**kargs)
    var_list = VarList()
    for arg in args:
        if isinstance(arg, netsnmp.client.Varbind):
            var_list.append(arg)
        else:
            var_list.append(Varbind(arg))
    res = sess.getbulk(nonrepeaters, maxrepetitions, var_list)
    return res

def snmpwalk(*args, **kargs):
    sess = Session(**kargs)
    if isinstance(args[0], netsnmp.client.VarList):
        var_list = args[0]
    else:
        var_list = VarList()
        for arg in args:
            if isinstance(arg, netsnmp.client.Varbind):
                var_list.append(arg)
            else:
                var_list.append(Varbind(arg))
    res = sess.walk(var_list)
    return res
    
