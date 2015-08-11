# $Id: mod_run.py 2078 2008-06-27 21:12:12Z nanang $
import imp
import sys

from inc_cfg import *

# Read configuration
cfg_file = imp.load_source("cfg_file", ARGS[1])

# Here where it all comes together
test = cfg_file.test_param
