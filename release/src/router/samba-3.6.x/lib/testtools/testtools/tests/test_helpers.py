# Copyright (c) 2010 Jonathan M. Lange. See LICENSE for details.

from testtools import TestCase
from testtools.helpers import (
    try_import,
    try_imports,
    )
from testtools.matchers import (
    Equals,
    Is,
    )


class TestTryImport(TestCase):

    def test_doesnt_exist(self):
        # try_import('thing', foo) returns foo if 'thing' doesn't exist.
        marker = object()
        result = try_import('doesntexist', marker)
        self.assertThat(result, Is(marker))

    def test_None_is_default_alternative(self):
        # try_import('thing') returns None if 'thing' doesn't exist.
        result = try_import('doesntexist')
        self.assertThat(result, Is(None))

    def test_existing_module(self):
        # try_import('thing', foo) imports 'thing' and returns it if it's a
        # module that exists.
        result = try_import('os', object())
        import os
        self.assertThat(result, Is(os))

    def test_existing_submodule(self):
        # try_import('thing.another', foo) imports 'thing' and returns it if
        # it's a module that exists.
        result = try_import('os.path', object())
        import os
        self.assertThat(result, Is(os.path))

    def test_nonexistent_submodule(self):
        # try_import('thing.another', foo) imports 'thing' and returns foo if
        # 'another' doesn't exist.
        marker = object()
        result = try_import('os.doesntexist', marker)
        self.assertThat(result, Is(marker))

    def test_object_from_module(self):
        # try_import('thing.object') imports 'thing' and returns
        # 'thing.object' if 'thing' is a module and 'object' is not.
        result = try_import('os.path.join')
        import os
        self.assertThat(result, Is(os.path.join))


class TestTryImports(TestCase):

    def test_doesnt_exist(self):
        # try_imports('thing', foo) returns foo if 'thing' doesn't exist.
        marker = object()
        result = try_imports(['doesntexist'], marker)
        self.assertThat(result, Is(marker))

    def test_fallback(self):
        result = try_imports(['doesntexist', 'os'])
        import os
        self.assertThat(result, Is(os))

    def test_None_is_default_alternative(self):
        # try_imports('thing') returns None if 'thing' doesn't exist.
        e = self.assertRaises(
            ImportError, try_imports, ['doesntexist', 'noreally'])
        self.assertThat(
            str(e),
            Equals("Could not import any of: doesntexist, noreally"))

    def test_existing_module(self):
        # try_imports('thing', foo) imports 'thing' and returns it if it's a
        # module that exists.
        result = try_imports(['os'], object())
        import os
        self.assertThat(result, Is(os))

    def test_existing_submodule(self):
        # try_imports('thing.another', foo) imports 'thing' and returns it if
        # it's a module that exists.
        result = try_imports(['os.path'], object())
        import os
        self.assertThat(result, Is(os.path))

    def test_nonexistent_submodule(self):
        # try_imports('thing.another', foo) imports 'thing' and returns foo if
        # 'another' doesn't exist.
        marker = object()
        result = try_imports(['os.doesntexist'], marker)
        self.assertThat(result, Is(marker))

    def test_fallback_submodule(self):
        result = try_imports(['os.doesntexist', 'os.path'])
        import os
        self.assertThat(result, Is(os.path))


def test_suite():
    from unittest import TestLoader
    return TestLoader().loadTestsFromName(__name__)
