# Copyright (C) 2010 Nominum, Inc.
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose with or without fee is hereby granted,
# provided that the above copyright notice and this permission notice
# appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND NOMINUM DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL NOMINUM BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
# OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import unittest

import dns.dnssec
import dns.name
import dns.rdata
import dns.rdataclass
import dns.rdatatype
import dns.rrset

abs_dnspython_org = dns.name.from_text('dnspython.org')

abs_keys = { abs_dnspython_org :
             dns.rrset.from_text('dnspython.org.', 3600, 'IN', 'DNSKEY',
                                 '257 3 5 AwEAAenVTr9L1OMlL1/N2ta0Qj9LLLnnmFWIr1dJoAsWM9BQfsbV7kFZ XbAkER/FY9Ji2o7cELxBwAsVBuWn6IUUAJXLH74YbC1anY0lifjgt29z SwDzuB7zmC7yVYZzUunBulVW4zT0tg1aePbpVL2EtTL8VzREqbJbE25R KuQYHZtFwG8S4iBxJUmT2Bbd0921LLxSQgVoFXlQx/gFV2+UERXcJ5ce iX6A6wc02M/pdg/YbJd2rBa0MYL3/Fz/Xltre0tqsImZGxzi6YtYDs45 NC8gH+44egz82e2DATCVM1ICPmRDjXYTLldQiWA2ZXIWnK0iitl5ue24 7EsWJefrIhE=',
                                 '256 3 5 AwEAAdSSghOGjU33IQZgwZM2Hh771VGXX05olJK49FxpSyuEAjDBXY58 LGU9R2Zgeecnk/b9EAhFu/vCV9oECtiTCvwuVAkt9YEweqYDluQInmgP NGMJCKdSLlnX93DkjDw8rMYv5dqXCuSGPlKChfTJOLQxIAxGloS7lL+c 0CTZydAF')
         }

rel_keys = { dns.name.empty :
             dns.rrset.from_text('@', 3600, 'IN', 'DNSKEY',
                                 '257 3 5 AwEAAenVTr9L1OMlL1/N2ta0Qj9LLLnnmFWIr1dJoAsWM9BQfsbV7kFZ XbAkER/FY9Ji2o7cELxBwAsVBuWn6IUUAJXLH74YbC1anY0lifjgt29z SwDzuB7zmC7yVYZzUunBulVW4zT0tg1aePbpVL2EtTL8VzREqbJbE25R KuQYHZtFwG8S4iBxJUmT2Bbd0921LLxSQgVoFXlQx/gFV2+UERXcJ5ce iX6A6wc02M/pdg/YbJd2rBa0MYL3/Fz/Xltre0tqsImZGxzi6YtYDs45 NC8gH+44egz82e2DATCVM1ICPmRDjXYTLldQiWA2ZXIWnK0iitl5ue24 7EsWJefrIhE=',
                                 '256 3 5 AwEAAdSSghOGjU33IQZgwZM2Hh771VGXX05olJK49FxpSyuEAjDBXY58 LGU9R2Zgeecnk/b9EAhFu/vCV9oECtiTCvwuVAkt9YEweqYDluQInmgP NGMJCKdSLlnX93DkjDw8rMYv5dqXCuSGPlKChfTJOLQxIAxGloS7lL+c 0CTZydAF')
         }

when = 1290250287

abs_soa = dns.rrset.from_text('dnspython.org.', 3600, 'IN', 'SOA',
                              'howl.dnspython.org. hostmaster.dnspython.org. 2010020047 3600 1800 604800 3600')

abs_other_soa = dns.rrset.from_text('dnspython.org.', 3600, 'IN', 'SOA',
                                    'foo.dnspython.org. hostmaster.dnspython.org. 2010020047 3600 1800 604800 3600')

abs_soa_rrsig = dns.rrset.from_text('dnspython.org.', 3600, 'IN', 'RRSIG',
                                    'SOA 5 2 3600 20101127004331 20101119213831 61695 dnspython.org. sDUlltRlFTQw5ITFxOXW3TgmrHeMeNpdqcZ4EXxM9FHhIlte6V9YCnDw t6dvM9jAXdIEi03l9H/RAd9xNNW6gvGMHsBGzpvvqFQxIBR2PoiZA1mX /SWHZFdbt4xjYTtXqpyYvrMK0Dt7bUYPadyhPFCJ1B+I8Zi7B5WJEOd0 8vs=')

rel_soa = dns.rrset.from_text('@', 3600, 'IN', 'SOA',
                              'howl hostmaster 2010020047 3600 1800 604800 3600')

rel_other_soa = dns.rrset.from_text('@', 3600, 'IN', 'SOA',
                                    'foo hostmaster 2010020047 3600 1800 604800 3600')

rel_soa_rrsig = dns.rrset.from_text('@', 3600, 'IN', 'RRSIG',
                                    'SOA 5 2 3600 20101127004331 20101119213831 61695 @ sDUlltRlFTQw5ITFxOXW3TgmrHeMeNpdqcZ4EXxM9FHhIlte6V9YCnDw t6dvM9jAXdIEi03l9H/RAd9xNNW6gvGMHsBGzpvvqFQxIBR2PoiZA1mX /SWHZFdbt4xjYTtXqpyYvrMK0Dt7bUYPadyhPFCJ1B+I8Zi7B5WJEOd0 8vs=')

sep_key = dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.DNSKEY,
                              '257 3 5 AwEAAenVTr9L1OMlL1/N2ta0Qj9LLLnnmFWIr1dJoAsWM9BQfsbV7kFZ XbAkER/FY9Ji2o7cELxBwAsVBuWn6IUUAJXLH74YbC1anY0lifjgt29z SwDzuB7zmC7yVYZzUunBulVW4zT0tg1aePbpVL2EtTL8VzREqbJbE25R KuQYHZtFwG8S4iBxJUmT2Bbd0921LLxSQgVoFXlQx/gFV2+UERXcJ5ce iX6A6wc02M/pdg/YbJd2rBa0MYL3/Fz/Xltre0tqsImZGxzi6YtYDs45 NC8gH+44egz82e2DATCVM1ICPmRDjXYTLldQiWA2ZXIWnK0iitl5ue24 7EsWJefrIhE=')

good_ds = dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.DS,
                              '57349 5 2 53A79A3E7488AB44FFC56B2D1109F0699D1796DD977E72108B841F96 E47D7013')

when2 = 1290425644

abs_example = dns.name.from_text('example')

abs_dsa_keys = { abs_example :
                 dns.rrset.from_text('example.', 86400, 'IN', 'DNSKEY',
                                     '257 3 3 CI3nCqyJsiCJHTjrNsJOT4RaszetzcJPYuoH3F9ZTVt3KJXncCVR3bwn 1w0iavKljb9hDlAYSfHbFCp4ic/rvg4p1L8vh5s8ToMjqDNl40A0hUGQ Ybx5hsECyK+qHoajilUX1phYSAD8d9WAGO3fDWzUPBuzR7o85NiZCDxz yXuNVfni0uhj9n1KYhEO5yAbbruDGN89wIZcxMKuQsdUY2GYD93ssnBv a55W6XRABYWayKZ90WkRVODLVYLSn53Pj/wwxGH+XdhIAZJXimrZL4yl My7rtBsLMqq8Ihs4Tows7LqYwY7cp6y/50tw6pj8tFqMYcPUjKZV36l1 M/2t5BVg3i7IK61Aidt6aoC3TDJtzAxg3ZxfjZWJfhHjMJqzQIfbW5b9 q1mjFsW5EUv39RaNnX+3JWPRLyDqD4pIwDyqfutMsdk/Py3paHn82FGp CaOg+nicqZ9TiMZURN/XXy5JoXUNQ3RNvbHCUiPUe18KUkY6mTfnyHld 1l9YCWmzXQVClkx/hOYxjJ4j8Ife58+Obu5X',
                                     '256 3 3 CJE1yb9YRQiw5d2xZrMUMR+cGCTt1bp1KDCefmYKmS+Z1+q9f42ETVhx JRiQwXclYwmxborzIkSZegTNYIV6mrYwbNB27Q44c3UGcspb3PiOw5TC jNPRYEcdwGvDZ2wWy+vkSV/S9tHXY8O6ODiE6abZJDDg/RnITyi+eoDL R3KZ5n/V1f1T1b90rrV6EewhBGQJpQGDogaXb2oHww9Tm6NfXyo7SoMM pbwbzOckXv+GxRPJIQNSF4D4A9E8XCksuzVVdE/0lr37+uoiAiPia38U 5W2QWe/FJAEPLjIp2eTzf0TrADc1pKP1wrA2ASpdzpm/aX3IB5RPp8Ew S9U72eBFZJAUwg635HxJVxH1maG6atzorR566E+e0OZSaxXS9o1o6QqN 3oPlYLGPORDiExilKfez3C/x/yioOupW9K5eKF0gmtaqrHX0oq9s67f/ RIM2xVaKHgG9Vf2cgJIZkhv7sntujr+E4htnRmy9P9BxyFxsItYxPI6Z bzygHAZpGhlI/7ltEGlIwKxyTK3ZKBm67q7B')
                 }

abs_dsa_soa = dns.rrset.from_text('example.', 86400, 'IN', 'SOA',
                                  'ns1.example. hostmaster.example. 2 10800 3600 604800 86400')

abs_other_dsa_soa = dns.rrset.from_text('example.', 86400, 'IN', 'SOA',
                                        'ns1.example. hostmaster.example. 2 10800 3600 604800 86401')

abs_dsa_soa_rrsig = dns.rrset.from_text('example.', 86400, 'IN', 'RRSIG',
                                        'SOA 3 1 86400 20101129143231 20101122112731 42088 example. CGul9SuBofsktunV8cJs4eRs6u+3NCS3yaPKvBbD+pB2C76OUXDZq9U=')

example_sep_key = dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.DNSKEY,
                                      '257 3 3 CI3nCqyJsiCJHTjrNsJOT4RaszetzcJPYuoH3F9ZTVt3KJXncCVR3bwn 1w0iavKljb9hDlAYSfHbFCp4ic/rvg4p1L8vh5s8ToMjqDNl40A0hUGQ Ybx5hsECyK+qHoajilUX1phYSAD8d9WAGO3fDWzUPBuzR7o85NiZCDxz yXuNVfni0uhj9n1KYhEO5yAbbruDGN89wIZcxMKuQsdUY2GYD93ssnBv a55W6XRABYWayKZ90WkRVODLVYLSn53Pj/wwxGH+XdhIAZJXimrZL4yl My7rtBsLMqq8Ihs4Tows7LqYwY7cp6y/50tw6pj8tFqMYcPUjKZV36l1 M/2t5BVg3i7IK61Aidt6aoC3TDJtzAxg3ZxfjZWJfhHjMJqzQIfbW5b9 q1mjFsW5EUv39RaNnX+3JWPRLyDqD4pIwDyqfutMsdk/Py3paHn82FGp CaOg+nicqZ9TiMZURN/XXy5JoXUNQ3RNvbHCUiPUe18KUkY6mTfnyHld 1l9YCWmzXQVClkx/hOYxjJ4j8Ife58+Obu5X')

example_ds_sha1 = dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.DS,
                                      '18673 3 1 71b71d4f3e11bbd71b4eff12cde69f7f9215bbe7')

example_ds_sha256 = dns.rdata.from_text(dns.rdataclass.IN, dns.rdatatype.DS,
                                        '18673 3 2 eb8344cbbf07c9d3d3d6c81d10c76653e28d8611a65e639ef8f716e4e4e5d913')

class DNSSECValidatorTestCase(unittest.TestCase):

    def testAbsoluteRSAGood(self):
        dns.dnssec.validate(abs_soa, abs_soa_rrsig, abs_keys, None, when)

    def testAbsoluteRSABad(self):
        def bad():
            dns.dnssec.validate(abs_other_soa, abs_soa_rrsig, abs_keys, None,
                                when)
        self.failUnlessRaises(dns.dnssec.ValidationFailure, bad)

    def testRelativeRSAGood(self):
        dns.dnssec.validate(rel_soa, rel_soa_rrsig, rel_keys,
                            abs_dnspython_org, when)

    def testRelativeRSABad(self):
        def bad():
            dns.dnssec.validate(rel_other_soa, rel_soa_rrsig, rel_keys,
                                abs_dnspython_org, when)
        self.failUnlessRaises(dns.dnssec.ValidationFailure, bad)

    def testMakeSHA256DS(self):
        ds = dns.dnssec.make_ds(abs_dnspython_org, sep_key, 'SHA256')
        self.failUnless(ds == good_ds)

    def testAbsoluteDSAGood(self):
        dns.dnssec.validate(abs_dsa_soa, abs_dsa_soa_rrsig, abs_dsa_keys, None,
                            when2)

    def testAbsoluteDSABad(self):
        def bad():
            dns.dnssec.validate(abs_other_dsa_soa, abs_dsa_soa_rrsig,
                                abs_dsa_keys, None, when2)
        self.failUnlessRaises(dns.dnssec.ValidationFailure, bad)

    def testMakeExampleSHA1DS(self):
        ds = dns.dnssec.make_ds(abs_example, example_sep_key, 'SHA1')
        self.failUnless(ds == example_ds_sha1)

    def testMakeExampleSHA256DS(self):
        ds = dns.dnssec.make_ds(abs_example, example_sep_key, 'SHA256')
        self.failUnless(ds == example_ds_sha256)

if __name__ == '__main__':
    import_ok = False
    try:
        import Crypto.Util.number
        import_ok = True
    except:
        pass
    if import_ok:
        unittest.main()
    else:
        print 'skipping DNSSEC tests because pycrypto is not installed'
