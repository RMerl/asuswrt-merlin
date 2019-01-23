# Copyright (c) 2010 Jonathan M. Lange. See LICENSE for details.

__all__ = [
    'try_import',
    'try_imports',
    ]


def try_import(name, alternative=None):
    """Attempt to import `name`.  If it fails, return `alternative`.

    When supporting multiple versions of Python or optional dependencies, it
    is useful to be able to try to import a module.

    :param name: The name of the object to import, e.g. 'os.path' or
        'os.path.join'.
    :param alternative: The value to return if no module can be imported.
        Defaults to None.
    """
    module_segments = name.split('.')
    while module_segments:
        module_name = '.'.join(module_segments)
        try:
            module = __import__(module_name)
        except ImportError:
            module_segments.pop()
            continue
        else:
            break
    else:
        return alternative
    nonexistent = object()
    for segment in name.split('.')[1:]:
        module = getattr(module, segment, nonexistent)
        if module is nonexistent:
            return alternative
    return module


_RAISE_EXCEPTION = object()
def try_imports(module_names, alternative=_RAISE_EXCEPTION):
    """Attempt to import modules.

    Tries to import the first module in `module_names`.  If it can be
    imported, we return it.  If not, we go on to the second module and try
    that.  The process continues until we run out of modules to try.  If none
    of the modules can be imported, either raise an exception or return the
    provided `alternative` value.

    :param module_names: A sequence of module names to try to import.
    :param alternative: The value to return if no module can be imported.
        If unspecified, we raise an ImportError.
    :raises ImportError: If none of the modules can be imported and no
        alternative value was specified.
    """
    module_names = list(module_names)
    for module_name in module_names:
        module = try_import(module_name)
        if module:
            return module
    if alternative is _RAISE_EXCEPTION:
        raise ImportError(
            "Could not import any of: %s" % ', '.join(module_names))
    return alternative
