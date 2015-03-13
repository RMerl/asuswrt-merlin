from conf import rule


@rule(alias='SampleRuleAlias')
class SampleRule:
    def __init__(self, rule):
        # do rule initialization here
        # you may also need to implement a method the same name of this
        # class in server/protocol/protocol_server.py to apply this rule.
        self.rule = rule
