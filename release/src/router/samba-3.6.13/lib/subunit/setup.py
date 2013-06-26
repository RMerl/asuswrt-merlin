#!/usr/bin/env python
try:
    # If the user has setuptools / distribute installed, use it
    from setuptools import setup
except ImportError:
    # Otherwise, fall back to distutils.
    from distutils.core import setup
    extra = {}
else:
    extra = {
        'install_requires': [
            'testtools>=0.9.6',
        ]
    }

try:
    # Assume we are in a distribution, which has PKG-INFO
    version_lines = [x for x in open('PKG-INFO').readlines()
                        if x.startswith('Version:')]
    version_line = version_lines and version_lines[-1] or 'VERSION = 0.0'
    VERSION = version_line.split(':')[1].strip()
 
except IOError:
    # Must be a development checkout, so use the Makefile
    version_lines = [x for x in open('Makefile').readlines()
                        if x.startswith('VERSION')]
    version_line = version_lines and version_lines[-1] or 'VERSION = 0.0'
    VERSION = version_line.split('=')[1].strip()


setup(
    name='python-subunit',
    version=VERSION,
    description=('Python implementation of subunit test streaming protocol'),
    long_description=open('README').read(),
    classifiers=[
        'Intended Audience :: Developers',
        'Programming Language :: Python',
        'Topic :: Software Development :: Testing',
    ],
    keywords='python test streaming',
    author='Robert Collins',
    author_email='subunit-dev@lists.launchpad.net',
    url='http://launchpad.net/subunit',
    packages=['subunit'],
    package_dir={'subunit': 'python/subunit'},
    scripts = [
        'filters/subunit2gtk',
        'filters/subunit2junitxml',
        'filters/subunit2pyunit',
        'filters/subunit-filter',
        'filters/subunit-ls',
        'filters/subunit-notify',
        'filters/subunit-stats',
        'filters/subunit-tags',
        'filters/tap2subunit',
    ],
    **extra
)
