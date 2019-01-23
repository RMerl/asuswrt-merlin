import os

# this file implements the mechanism of conf class auto-registration,
# don't modify this file if you have no idea what you're doing


def gen_hook():
    hook_table = {}

    class Wrapper:
        """
        Decorator class which implements the conf class registration.
        """
        def __init__(self, alias=None):
            self.alias = alias

        def __call__(self, cls):
            # register the class object with the name of the class
            hook_table[cls.__name__] = cls
            if self.alias:
                # also register the alias of the class
                hook_table[self.alias] = cls

            return cls

    def find_hook(name):
        try:
            return hook_table[name]
        except:
            raise AttributeError

    return Wrapper, find_hook

_register, find_conf = gen_hook()
hook = rule = _register

__all__ = ['hook', 'rule']

for module in os.listdir(os.path.dirname(__file__)):
    # import every module under this package except __init__.py,
    # so that the decorator `register` applies
    # (nothing happens if the script is not loaded)
    if module != '__init__.py' and module.endswith('.py'):
        module_name = module[:-3]
        mod = __import__('%s.%s' % (__name__, module_name),
                         globals(),
                         locals())
        __all__.append(module_name)
