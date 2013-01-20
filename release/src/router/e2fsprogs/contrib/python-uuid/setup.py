#!/usr/bin/python
from distutils.core import setup, Extension

uuid = Extension('e2fsprogs_uuid',
                 sources = ['uuid.c'],
                 libraries = ['uuid'])

setup (name = 'e2fsprogs_uuid',
       version = '1.0',
       description = 'This is python uuid interface',
       ext_modules = [uuid])
