from conf import hook

@hook(alias='Domains')
class Domains:
    def __init__(self, domains):
        self.domains = domains

    def __call__(self, test_obj):
        test_obj.domains = self.domains
