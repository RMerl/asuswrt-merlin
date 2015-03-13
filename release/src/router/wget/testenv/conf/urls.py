from conf import hook

""" Pre-Test Hook: URLS
This hook is used to define the paths of the files on the test server that wget
will send a request for. """


@hook(alias='Urls')
class URLs:
    def __init__(self, urls):
        self.urls = urls

    def __call__(self, test_obj):
        test_obj.urls = self.urls
