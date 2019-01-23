from functools import partial
import platform
from os import getenv
import sys

""" This module allows printing coloured output to the terminal when running a
Wget Test under certain conditions.
The output is coloured only on Linux systems. This is because coloured output
in the terminal on Windows requires too much effort for what is simply a
convenience. This might work on OSX terminals, but without a confirmation, it
remains unsupported.

Another important aspect is that the coloured output is printed only if the
environment variable MAKE_CHECK is not set. This variable is set when running
the test suite through, `make check`. In that case, the output is not only
printed to the terminal but also copied to a log file where the ANSI escape
codes on;y add clutter. """


T_COLORS = {
    'PURPLE': '\033[95m',
    'BLUE':   '\033[94m',
    'GREEN':  '\033[92m',
    'YELLOW': '\033[93m',
    'RED':    '\033[91m',
    'ENDC':   '\033[0m'
}

system = True if platform.system() in ('Linux', 'Darwin') else False
check = False if getenv("MAKE_CHECK") == 'True' else True


def printer(color, string):
    if sys.stdout.isatty() and system and check:
        print(T_COLORS.get(color) + string + T_COLORS.get('ENDC'))
    else:
        print(string)


print_blue = partial(printer, 'BLUE')
print_red = partial(printer, 'RED')
print_green = partial(printer, 'GREEN')
print_purple = partial(printer, 'PURPLE')
print_yellow = partial(printer, 'YELLOW')

# vim: set ts=8 sw=3 tw=80 et :
