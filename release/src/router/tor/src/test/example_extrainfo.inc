static const char EX_EI_MINIMAL[] =
  "extra-info bob 3E1B2DC141F2B7C6A0F3C4ED9A14A9C35762E24B\n"
  "published 2014-10-05 20:07:00\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "K5GAkVjpUlofL78NIOE1VDxFn8yYbHK50rVuZG2HxqG/727bon+uMprv4MHjfDcP\n"
  "V3l9u1uUdGiUPOl8j+hRNw4z/ODeCj/24r2+L32MTjyfUhK49Ld2IlK9iZKlgKYi\n"
  "zyoatxdAjU8Xc5WPX692HO4/R9CGLsUfYcEEFU2R3EA=\n"
  "-----END SIGNATURE-----\n"
  ;

static const char EX_EI_MINIMAL_FP[] = "3E1B2DC141F2B7C6A0F3C4ED9A14A9C35762E24B";
static const char EX_EI_MINIMAL_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBALSppIF3t3wOAm4fzxRvK+q/wh1gGAWwS0JEn8d+c/x+rt1oQabGkqsB\n"
  "GU6rz1z1AN02W0P2+EcyJQVBjGR3gHQNoDGx0KIdnr3caGAw3XmQXrJLPaViEk28\n"
  "RJMxx6umpP27YKSyEMHgVTDXblKImT0mE7fVOx8tD0EWRYazmp4NAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n";

static const char EX_EI_MAXIMAL[] =
  "extra-info bob FF8248FE780A7236D3FA5D62DEA642055135F942\n"
  "published 2014-10-05 20:07:00\n"
  "opt foobarbaz\n"
  "read-history 900 1,2,3\n"
  "write-history 900 1,2,3\n"
  "dirreq-v2-ips 1\n"
  "dirreq-v3-ips 100\n"
  "dirreq-v3-reqs blahblah\n"
  "dirreq-v2-share blahblah\n"
  "dirreq-v3-share blahblah\n"
  "dirreq-v2-resp djfkdj\n"
  "dirreq-v3-resp djfkdj\n"
  "dirreq-v2-direct-dl djfkdj\n"
  "dirreq-v3-direct-dl djfkdj\n"
  "dirreq-v2-tunneled-dl djfkdj\n"
  "dirreq-v3-tunneled-dl djfkdj\n"
  "dirreq-stats-end foobar\n"
  "entry-ips jfsdfds\n"
  "entry-stats-end ksdflkjfdkf\n"
  "cell-stats-end FOO\n"
  "cell-processed-cells FOO\n"
  "cell-queued-cells FOO\n"
  "cell-time-in-queue FOO\n"
  "cell-circuits-per-decile FOO\n"
  "exit-stats-end FOO\n"
  "exit-kibibytes-written FOO\n"
  "exit-kibibytes-read FOO\n"
  "exit-streams-opened FOO\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "ZO79bLlWVNIruCnWW9duDcOKydPWbL5DfrpUv5IRLF4MMFoacMUdJPDUs9e+wY2C\n"
  "zndHe6i2JK7yKJj+uCOSC8cx61OLG+kVxMLJ/qhA4H5thrYb+GpzMKwbHzQc3PTH\n"
  "zHRzj041iWXTL7/DMaQlpJOBoac/wTSIKzoV2B00jBw=\n"
  "-----END SIGNATURE-----\n"
  ;

static const char EX_EI_MAXIMAL_FP[] = "FF8248FE780A7236D3FA5D62DEA642055135F942";
static const char EX_EI_MAXIMAL_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBANSpkYhHUW1EqodY4d3JRbvEM1vjjR/vEE8gjONiJ5t2Sten53jzt8bh\n"
  "8/VJn7pQGs8zR5CIxCw4P68xMtZJJedS3hhjqubheOE/yW1DtpkiCf+zVEaLpeA8\n"
  "fYQChkRICnR/BZd4W9bbohLVII5ym2PaJt2ihB3FeVZIsGXm4wxhAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n";

static const char EX_EI_BAD_SIG1[] =
  "extra-info bob 3E1B2DC141F2B7C6A0F3C4ED9A14A9C35762E24B\n"
  "published 2014-10-05 20:07:00\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "K5GAkVjpUlofL78NIOE1VDxFn8yYbHK50rVuZG2HxqG/727bon+uMprv4MHjfDcP\n"
  "V3l9u1uUdGiUPOl8j+hXXw4z/ODeCj/24r2+L32MTjyfUhK49Ld2IlK9iZKlgKYi\n"
  "zyoatxdAjU8Xc5WPX692HO4/R9CGLsUfYcEEFU2R3EA=\n"
  "-----END SIGNATURE-----\n"
  ;

static const char EX_EI_BAD_SIG2[] =
  "extra-info bob 3E1B2DC141F2B7C6A0F3C4ED9A14A9C35762E24B\n"
  "published 2014-10-06 20:07:00\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "K5GAkVjpUlofL78NIOE1VDxFn8yYbHK50rVuZG2HxqG/727bon+uMprv4MHjfDcP\n"
  "V3l9u1uUdGiUPOl8j+hRNw4z/ODeCj/24r2+L32MTjyfUhK49Ld2IlK9iZKlgKYi\n"
  "zyoatxdAjU8Xc5WPX692HO4/R9CGLsUfYcEEFU2R3EA=\n"
  "-----END SIGNATURE-----\n"
  ;

static const char EX_EI_BAD_SIG3[] =
  "extra-info bob 3E1B2DC141F2B7C6A0F3C4ED9A14A9C35762E24B\n"
  "published 2014-10-05 20:07:00\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "K5GAkVjpUlofL78NIOE1VDxFn8yYbHK50rVuZG2HxqG/727bon+uMprv4MHjfDcP\n"
  "V3l9u1uUdGiUPOl8j+hRNw4z/ODeCj/24r2+L32MTjyfUhK49Ld2IlK9iZKlgKYi\n"
  "zyoatxdAjU8Xc5WPX692HO4/R9CGLsUfYcEEFU2=\n"
  "-----END SIGNATURE-----\n"
  ;

static const char EX_EI_BAD_FP[] =
  "extra-info bob C34293303F0F1E42CB14E593717B834E8E53797D8888\n"
  "published 2014-10-05 20:07:00\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "IDA8ryUYeMx7+Au/xQmX7Y8fXksoHUOXmePND2JYM4rPfishQJ1LpQ15KrolOZDH\n"
  "FVIk3RmCefNlJeS1/UgWPcU8u2nGw1YQuRBHF4ViTmZ0OevI1pTsSApl4+oIx2dy\n"
  "DGgCQmKfMbaOixIK8Ioh1Z2NUfMkjbUUE2WWgFTAsac=\n"
  "-----END SIGNATURE-----\n"
  ;

static const char EX_EI_BAD_FP_FP[] = "C34293303F0F1E42CB14E593717B834E8E53797D";
static const char EX_EI_BAD_FP_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBAKXMSbif4fG+BW/5lIq5V1tMRondIUfKiNizp0E6EcBw5LvYfQV6zrj8\n"
  "HmMFbB/WGf9XGVMxIBzxzeQBRvCQJh+0QH7+ju5/isIHJZsACMILepr6ywmCcjVU\n"
  "iYRtC8zGQLqfkf2cNoo7AhcI5i/YzyW2u1zmbPX5J+8sUErfxydbAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n";

static const char EX_EI_BAD_NICKNAME[] =
  "extra-info bobhasaverylongnameandidontthinkweshouldlethim A4EA2389A52459B3F7C7121A46012F098BDFC2A4\n"
  "published 2014-10-05 20:07:00\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "e2wLJFThRMGawxKrQPuH2XCLek/LJsg4XOB8waAjE0xdHOrzjur9x1jIxy7DVU6t\n"
  "z1edbIoL24qucMJvFy2xjSQhFRX4OsyNc0nWr3LfJnTW9aEmxuwXM+mltUD2uFN1\n"
  "2vYOIQjUmJwS2yfeSKnhXEl2PWVUmgzYL3r4S5kHco4=\n"
  "-----END SIGNATURE-----\n"
  ;

static const char EX_EI_BAD_NICKNAME_FP[] = "A4EA2389A52459B3F7C7121A46012F098BDFC2A4";
static const char EX_EI_BAD_NICKNAME_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBAKfq7oxD1kMu1+zeG2UVXN4vOu6FDp0V/olA3ttmXpUCgCiBxWTgtwNl\n"
  "nPf0HcKMaCp/0D9XrbhvIoOsg0OTf1TcJfGsA/zPG7jrWYa4xhD50KYvty9EINK9\n"
  "/UBWNSyXCFDMqnddb/LZ8+VgttmxfYkpeRzSSmDijN3RbOvYJhhBAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n";

static const char EX_EI_BAD_TOKENS[] =
  "extra-info bob 6F314FB01A31162BD5E473D4977AC570DC5B86BB\n"
  "published 2014-10-05 20:07:00\n"
  "published 2014-10-05 20:07:00\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "lhRIafrkKoQmnUoBLiq4XC8XKXrleGJZ5vefkLcgjOJ5IffsvVdIA7Vqq/ISbPrG\n"
  "b/Zs0sJNL6naHPxJBglgHJqksSyiYHaeOetXg2Rb+vZ1v2S5BrVgk1nPMDhyIzqc\n"
  "zU7eCxFf/1sXKtWlEKxGdX4LmVfnIln5aI31Bc4xRrE=\n"
  "-----END SIGNATURE-----\n"
  ;

static const char EX_EI_BAD_TOKENS_FP[] =
  "6F314FB01A31162BD5E473D4977AC570DC5B86BB";
static const char EX_EI_BAD_TOKENS_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBAL7Z8tz45Tb4tnEFS2sAyjubBV/giSfZdmXRkDV8Jo4xqWqhWFJn7+zN\n"
  "AXBWBThGeVH2WXrpz5seNJXgZJPxMTMsrnSCGcRXZw0Npti2MkLuQ6+prZa+OPwE\n"
  "OyC6jivtAaY/o9iYQjDC2avLXD3N4LvoygyF418KnNcjbzuFygffAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n";

static const char EX_EI_BAD_START[] =
  "published 2014-10-05 20:07:00\n"
  "extra-info bob 5CCCACE71A9BDB5E8E0C942AB3407452350434C0\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "BOiWgexqCAMZ8uyJ7jwBwRkz7Ox8cT4BImkmkV3bQiZgcWvPiYA3EnCm2ye48Ldg\n"
  "zBST2p6zJM5o4MEDYGMxfViS86Abj/z7DOY1gtLhjmAaVjIIpXc3koxEZtzCecqy\n"
  "JQz6xEg9/KoEuoT0DRrfYQ+KtQfzBDWrotfOvEa1rvc=\n"
  "-----END SIGNATURE-----\n"
  ;

static const char EX_EI_BAD_START_FP[] = "5CCCACE71A9BDB5E8E0C942AB3407452350434C0";
static const char EX_EI_BAD_START_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBAK2OCIfM6Cin/lq99Z3w9tl6HeyGlkBZu9MQEPHxqGIHTq78lIC1UkrC\n"
  "6NTqlrHBV9dmfzdwJn4GgMWsCZafL0FPIH3HNyNKUxLgyjixyKljHx2rfErSfOxI\n"
  "bMoOGBKv7m1EZZ0O5uG9ly9MBiNGdJyLdlnVvH7wSCnYciizpO4lAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n";

static const char EX_EI_BAD_PUBLISHED[] =
  "extra-info bob E67C477E3536BDE348BD407426D9679E5AE0BC16\n"
  "published 2014-99-05 20:07:00\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "l45IziBaXRKIjPAIUogMFNjQgH6k6Vm0+6r5+oByr4sP+B3ufNdUA6+WqBs43F0Z\n"
  "IqcJiT9nFn0DuNd/liOyOCixppDLx5h5NrhoGqcT3ySADEEXhzjlmc35TI3YBNVO\n"
  "v98fotmwIEg9YRWVGPg6XuIn2PRyiboFyjUpaYGCV0Q=\n"
  "-----END SIGNATURE-----\n"
  ;

static const char EX_EI_BAD_PUBLISHED_FP[] = "E67C477E3536BDE348BD407426D9679E5AE0BC16";
static const char EX_EI_BAD_PUBLISHED_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBAL7q8GEI18iv8Fo0QbNHmFatQ2FNacalPldpmKUdMJYEVZtdOR0nhcrY\n"
  "BvG6303md3INygg+KP49RvWEJR/cU4RZ9QfHpORxH2OocMyRedw2rLex2E7jNNSi\n"
  "52yd1sHFYI8ZQ4aff+ZHUjJUGKRyqpbc8okVbq/Rl7vug0dd12eHAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n";

static const char EX_EI_GOOD_ED_EI[] =
  "extra-info emma A692FE045C32B5E3A54B52882EF678A9DAC46A73\n"
  "identity-ed25519\n"
  "-----BEGIN ED25519 CERT-----\n"
  "AQQABf55AYgHn/OKR8GHBlscN5VkO73wA9jSci8QgTM30615ZT44AQAgBAC08woT\n"
  "MBZpKzRcaoEJhEG7+RmuYtnB2+nODk9IRIs8ZoyYPTZ6dLzI+MLMmtzUuo/Wmvw0\n"
  "PflTyCb2RlWitOEhAErWH3Z9UmYGnzM/COId0Fe3ScSriyvRoFnJY1+GVAQ=\n"
  "-----END ED25519 CERT-----\n"
  "published 2014-10-05 20:07:00\n"
  "router-sig-ed25519 a7K8nwfg+HrdlSGQwr9rnLBq0qozkyZZs6d6aiLEiXGdhV1r9KJncmlQ5SNoY/zMQlyQm8EV5rCyBiVliKQ1Bw\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "GvmCmIGgbC1DeawRyRuChy62VmBOG0EviryG/a2qSZiFy0iPPwqSp5ZyZDQEIEId\n"
  "kkk1zPzK1+S3fmgOAXyXGH0r4YFkoLGnhMk07BoEwi6HEXzjJsabmcNkOHfaOWgs\n"
  "/5nvnLfcmxL4c6FstZ7t9VQpE06y3GU0zwBeIy1qjp0=\n"
  "-----END SIGNATURE-----\n"
  "\n"
  "\n"
  ;
static const char EX_EI_GOOD_ED_EI_FP[] =
  "A692FE045C32B5E3A54B52882EF678A9DAC46A73";
static const char EX_EI_GOOD_ED_EI_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBAM3jdYwjwGxDWYj/vyFkQT7RgeCNIn89Ei6D2+L/fdtFnqrMXOreFFHL\n"
  "C7CK2v2uN3v+uXxfb5lADz3NcalxJrCfGTGtaBk7PwMZraTSh2luFKOvSRBQCmB1\n"
  "yD5N0QqnIhBJoGr6NITpbWyiTKWvYLjl9PZd9af8e8jQCAa5P1j1AgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n"
  ;

static const char EX_EI_ED_MISSING_SIG[] =
  "extra-info rachel 2A7521497B91A8437021515308A47491164EDBA1\n"
  "identity-ed25519\n"
  "-----BEGIN ED25519 CERT-----\n"
  "AQQABf55AT2/T71LFYHiI1ppwNiuaewIu2Hq+GWWQ85O8gpWcUxeAQAgBAC2dgYu\n"
  "moxhtuip7GVlthT9iomZKba1IllVa7uE1u2uO9BUYZQWXciFt7OnNzMH5mlffwxB\n"
  "1dWCl+G5nbOsV5jYLbfhrF5afZotf+EQTfob4cCH79AV223LPcySbTHTtQ4=\n"
  "-----END ED25519 CERT-----\n"
  "published 2014-10-05 20:07:00\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "oypRD2IZQ5EttOE8dvofrW80nnBfijSkvYzBrM6H4KVeayRYvWfmi96dYO6ybMqm\n"
  "Yp7Gs3ngqeeNdfHtkRPuQVUXUGYZgBTvYItuagnFlFgRqaHy0knwUIVOL35eqWYx\n"
  "xSbQKA7fglxEDMFs/RK7FRP4dWc731ZMt5wzzfJHZ8E=\n"
  "-----END SIGNATURE-----\n"
  "\n"
  "\n"
  ;
static const char EX_EI_ED_MISSING_SIG_FP[] =
  "2A7521497B91A8437021515308A47491164EDBA1";
static const char EX_EI_ED_MISSING_SIG_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBAOOB8ccxbtk2dB5FuKFhGndDcO6STNjB6KiG0b9X2QwKrOZMfmXSigto\n"
  "mtC1JfPTxECayRjLSiP/9UD8iTVvlcnc8mMWBGM12Pa/KoCZRn7McHI3JJ7n9lfn\n"
  "qw9+iZ9b/rBimzOb3W6k3uxzg9r8secdq4jJwTnwSjTObgxZtC8/AgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n"
  ;

static const char EX_EI_ED_MISSING_CERT[] =
  "extra-info lynne E88E43E86015345A323D93D825C33E4AD1028F65\n"
  "published 2014-10-05 20:07:00\n"
  "router-sig-ed25519 H4gKIKm5K9Pfkriy7SlMUD6BdYVp6B5mXKzR/rTyYlpH0tEZ4Fx2hlHNfNNdWXJieXzKZQZo8e7SOVzvrAC3CQ\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "dIrbQjK5T9t5KM8CpsMF85hh2i060oPIxzYQMgE1q4j99dtb/n7SE8nhj1Sjij4D\n"
  "7JvTjGdLHi3bFSxXaSmla0wxD9PUYFN7VsBQmwSaDrqrzJFb1SGwZuzW1IEZ7BBi\n"
  "H0czsxEteg5hcNRwISj5WVthuWmau9v13MijtZGSK40=\n"
  "-----END SIGNATURE-----\n"
  "\n"
  "\n"
  "\n"
  ;
static const char EX_EI_ED_MISSING_CERT_FP[] =
  "E88E43E86015345A323D93D825C33E4AD1028F65";
static const char EX_EI_ED_MISSING_CERT_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBALjA/geb0TR9rp/UPvLhABQpB0XUDYuZAnLkrv+i7AAV7FemTDveEGnc\n"
  "XdXNSusO1mHOquvr0YYKPhwauInxD56S8QOzLYiWWajGq8XHARQ33b4/9K2TUrAx\n"
  "W9HTHV1U1zrPlCJtrkbjxsYoHpUg5ljzM7FGYGY5xuvyHu18SQvzAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n"
  ;
static const char EX_EI_ED_BAD_CERT1[] =
  "extra-info marcie F78D8A655607D32281D02144817A4F1D26AE520F\n"
  "identity-ed25519\n"
  "-----BEGIN PLAGICAL SPELL-----\n"
  "aaaa\n"
  "-----END PLAGICAL SPELL\n"
  "published 2014-10-05 20:07:00\n"
  "router-sig-ed25519 KQJ+2AH7EkkjrD0RtDtUAIr+Vc7wndwILYnoUxFLSJiTP+5fMi54eFF/f1OgkG8gYyTh8phMij9WOxK/dsOpBg\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "XWD+P25AH6moi79j20Si3hqKGcJDws+FORL1MTu+GeJLV1mp5CR9N83UH4ffulcL\n"
  "CpSSBDL/j74HqapzW7QvBx3FilaNT55GvcobZDFK4TKkCEyEmcuWKpEceBS7JTTV\n"
  "SvwZeOObTjWPafELbsc/gI9Rh5Idwu7mZt3ZVntCGaQ=\n"
  "-----END SIGNATURE-----\n"
  "\n"
  ;
static const char EX_EI_ED_BAD_CERT1_FP[] =
  "F78D8A655607D32281D02144817A4F1D26AE520F";
static const char EX_EI_ED_BAD_CERT1_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBAMlR46JhxsCmWYtmIB/JjTV2TUYIhJLmHy+X7FfkK3ZVQvvl9/3GSXFL\n"
  "3USfyf3j34XLh8An7pJBi9LAHkIXgnRbglCud7dXoexabmC+c2mSbw5RnuxDGEwz\n"
  "krXUph/r2b+2UY1CgEt28nFigaHrIQbCmF4szFX/2GPYCLi5SrRNAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n"
  ;
static const char EX_EI_ED_BAD_CERT2[] =
  "extra-info jaeger 7C2B42E783C4E0EB0CC3BDB37385D16737BACFBD\n"
  "identity-ed25519\n"
  "-----BEGIN ED25519 CERT-----\n"
  "AQoABf55Acpw27GZBdwGCgawCj2F/DPadt8F/9DnEWywEew1Yi3qAOtLpCB8KXL7\n"
  "4w5deFW2RBg8qTondNSUvAmwYLbLjNXMmgA3+nkoJOP3fcmQMHz1jm5xzgs2lCVP\n"
  "t5txApaBIA4=\n"
  "-----END ED25519 CERT-----\n"
  "published 2014-10-05 20:07:00\n"
  "router-sig-ed25519 DRQ4MLOGosBbW8M+17klNu8uWVkPxErmmEYoSo6OuH2Tzrcs6sUY+8Xi2qLoV1SbOugJ214Htl0I+6ceag+vBA\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "DfdA+DbuN9nVJNujuSY5wNCDLk7Hfzkrde/sK0hVmZRvivtpF/Fy/dVQHHGNFY5i\n"
  "L1cESAgq9HLdbHU+hcc08XXxTIaGwvoklcJClcG3ENVBWkTXbJNT+ifr7chEagIi\n"
  "cVrtU6RVmzldSbyir8V/Z4S/Cm67gYAgjM5gfoFUqDs=\n"
  "-----END SIGNATURE-----\n"
  ;
static const char EX_EI_ED_BAD_CERT2_FP[] =
  "7C2B42E783C4E0EB0CC3BDB37385D16737BACFBD";
static const char EX_EI_ED_BAD_CERT2_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBALAM1F/0XJEsbxIQqb3+ObX/yGVnq9of8Q9sLsmxffD6hwVpCqnV3lTg\n"
  "iC6+xZ/bSlTGLPi0k8QLCaTmYxgKwmlMPpbQZ4kpZUrsb9flKdChMN7w8hd48pY9\n"
  "lu8QiAEgErsl5rCCJIHHjrxxM/Cnd0TnedRnj/Z2YqpNx/ggsmsRAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n"
  ;
static const char EX_EI_ED_BAD_SIG1[] =
  "extra-info vary 5AC3A538FEEFC6F9FCC5FA0CE64704396C30D62A\n"
  "identity-ed25519\n"
  "-----BEGIN ED25519 CERT-----\n"
  "AQQABf55AbPp++GrRb6WphSu+PkMaYsqY/beiLBmtiV3YP5i2JkKAQAgBABKXjg1\n"
  "aiz2JfQpNOG308i2EojnUAZEk0C0x9g2BAAXGL63sv3eO/qrlytsG1x2hkcamxFn\n"
  "LmfZBb/prqe1Vy4wABuhqWHAUtM29vXR6lpiCJeddt9Pa8XVy/tgWLX6TAw=\n"
  "-----END ED25519 CERT-----\n"
  "published 2014-10-05 20:07:00\n"
  "router-sig-ed25519 a7K8nwfg+HrdlSGQwr9rnLBq0qozkyZZs6d6aiLEiXGdhV1r9KJncmlQ5SNoY/zMQlyQm8EV5rCyBiVliKQ1Bw\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "xhZX8Qmgft51NJ7eMd4vrESzf/VdxDrBz7hgn8K+5bLtZUksG0s6s7IyGRYWQtp4\n"
  "/7oc9sYe3lcQiUN2K7DkeBDlL8Pcsl8aIlKuujWomCE3j0TIu+8XK6oJeo7eYic+\n"
  "IA7EwVbdZsKsW5/eJVzbX2eO0a5zyJ5RIYotFNYNCSE=\n"
  "-----END SIGNATURE-----\n"
  "\n"
  ;
static const char EX_EI_ED_BAD_SIG1_FP[] =
  "5AC3A538FEEFC6F9FCC5FA0CE64704396C30D62A";
static const char EX_EI_ED_BAD_SIG1_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBAMvb6SuoIkPfBkJgQuo5aQDepAs1kEETZ9VXotMlhB0JJikrqBrAAz+7\n"
  "rjIJ4JsBaeQuN0Z5ksXk2ebxtef7oMIUs37NfekLQHbNR0VsXkFXPEGmOAqpZjW0\n"
  "P524eHqybWYZTckvZtUvKI3xYGD6kEEkz4qmV6dcExU1OiAYO9jrAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n"
  ;
static const char EX_EI_ED_BAD_SIG2[] =
  "extra-info coward 7F1D4DD477E340C6D6B389FAC26EDC746113082F\n"
  "identity-ed25519\n"
  "-----BEGIN ED25519 CERT-----\n"
  "AQQABf56AZkSDiFZ1QaiLJhcKdFDE5Kei/sPaPEIEoPMGP4BvOVXAQAgBAAlRLzx\n"
  "U029tgIL9BRe47MVgcPJGy48db6ntzhjil7iOnWKT70z2LorUD5CZoLJs72TjB6r\n"
  "8+HYNyFLEM6dvytWZf9NA5gLdhogbFcUk/R3gbNepmCF7XoZjbhPIp8zOwg=\n"
  "-----END ED25519 CERT-----\n"
  "published 2014-10-05 20:07:00\n"
  "router-sig-ed25519 yfV+GySMIP1fw1oVa1C1de4XOWBqT4pUtEmSHq1h+WrLBNCh3/HZWvNC/denf2YVntuQrMLCJEv5ZaFKU+AIDQ\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "g+BWq69i9CP19va2cYMAXCQ6jK3IG0VmNYspjjUFgmFpJKGG6bHeOkuy1GXp47fG\n"
  "LzZ3OPfJLptxU5AOQDUUYf25hu9uSl6gyknCzsszFs5n6ticuNejvcpzw6UfO1LP\n"
  "5u+mGJlgpcMtmSraImDZrRipmZ3oRWvEULltlvzGQcQ=\n"
  "-----END SIGNATURE-----\n"
  "\n"
  ;
static const char EX_EI_ED_BAD_SIG2_FP[] =
  "7F1D4DD477E340C6D6B389FAC26EDC746113082F";
static const char EX_EI_ED_BAD_SIG2_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBALzOyfCEUZnvCyhlyMctPkdXg/XRE3Cr6QgyzdKf5kQbUiu2n0FgSHOX\n"
  "iP5gfq8sO9eVeTPZtjE7/+KiR8aQJECy+eoye+lpsfm3tXpLxnpOIgL4DlURxlo/\n"
  "rfCyv30SYBN9j62qgU9m6U2ydI0tH7/9Ep8yIY/QL8me8VAjLbf/AgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n"
  ;

static const char EX_EI_ED_MISPLACED_CERT[] =
  "extra-info msselene 3B788BD0CE348BC5CED48313307C78175EB6D0F3\n"
  "published 2014-10-05 20:07:00\n"
  "identity-ed25519\n"
  "-----BEGIN ED25519 CERT-----\n"
  "AQQABf55AWBcqjzLESDuLNGsqQ/tHn32XueXwj2fDlgEy/kQNVf/AQAgBAAFOegg\n"
  "XY1LR82xE9ohAYJxYpwJJw0YfXsBhGHqfakEoBtSgFJ3cQAUXZQX4lX6G8IxAlQB\n"
  "7Rj7dPQuQRUmqD1yyKb/ScBgCa8esxlhNlATz47kRNR38A3TcoJ4c1Zv6AE=\n"
  "-----END ED25519 CERT-----\n"
  "router-sig-ed25519 Q52JKH9/iMsr1jIPlWHHxakSBvyqjT1gzL944vad4OhzCZuNuAYGWyWSGzTb1DVmBqqbAUq73TiZKAz77YLNCQ\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "YplvAIwExGf5/L8AoroVQXtGm+26EffrxKBArMKn0zS1NOOie1p0oF/+qJg+rNWU\n"
  "6cv3Anf188EXGlkUOddavgVH8CQbvve2nHSfIAPxjgEX9QNXbM5CiaMwgpCewXnF\n"
  "UoNBVo5tydeLHVns15MBg/JNIxUQMd6svMoPp2WqmaE=\n"
  "-----END SIGNATURE-----\n"
  "\n"
  ;
static const char EX_EI_ED_MISPLACED_CERT_FP[] =
  "3B788BD0CE348BC5CED48313307C78175EB6D0F3";
static const char EX_EI_ED_MISPLACED_CERT_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBALTwNqhTprg1oC6bEbDqwIYBoER6prqUXQFbwbFDn+ekXhZj8vltgGwp\n"
  "aDGl9ceZWDKfi+reR6rZXjAJGctmv0VHkfe7maUX4FC/d2T8N8DvS+3IvJzFMpbT\n"
  "O0fFrDTrCSnPikqFfQWnlP8yoF5vO7wo0jRRY432fLRXg9WqVzdrAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n"
  ;
static const char EX_EI_ED_MISPLACED_SIG[] =
  "extra-info grazie 384E40A5DEED4AB1D8A74F1FCBDB18B7C24A8284\n"
  "identity-ed25519\n"
  "-----BEGIN ED25519 CERT-----\n"
  "AQQABf55AcGuIBoa6TBqD8Gg5atcwp/+r9ThxIBkULmPv9OSGhv+AQAgBACXH13y\n"
  "mUvdpcN6oRN1nX6mnH40LyfYR5um8xogJZk3oINse5cRNrfMgVWiBpDlJZAwlDDa\n"
  "lx99hzuZBong+CiOcnEvLMsBaVJmNTm5mpdetYclZpl0g8QEXznXXeRBMgM=\n"
  "-----END ED25519 CERT-----\n"
  "router-sig-ed25519 TxuO86dQ3pUaIY2raQ3hoDBmh4TTPC0OVgY98T5cf6Y+sHyiELCkkKQ3lqqXCjqnbTLr1/4riH980JoWPpR+Dw\n"
  "published 2014-10-05 20:07:00\n"
  "router-signature\n"
  "-----BEGIN SIGNATURE-----\n"
  "kV2CtArl1VF1nUSyHL00mO3nEdNxlQU5N7/hZNTd+45lej5Veb+6vb4ujelsFERJ\n"
  "YoxwIs6SuKAR4orQytCL0e+GgZsrg8zGTveEtMX/+u//OcCwQBYEevR5duBZjVw/\n"
  "yzpEHwdIdB2PPyDBLkf1VKnP7uDj059tXiQRWl7LXgE=\n"
  "-----END SIGNATURE-----\n"
  "\n"
  ;
static const char EX_EI_ED_MISPLACED_SIG_FP[] =
  "384E40A5DEED4AB1D8A74F1FCBDB18B7C24A8284";
static const char EX_EI_ED_MISPLACED_SIG_KEY[] =
  "-----BEGIN RSA PUBLIC KEY-----\n"
  "MIGJAoGBAK0HgOCG/6433VCrwz/vhk3cKmyOfenCp0GZ4DIUwPWt4DeyP4nTbN6T\n"
  "1HJ1H8+hXC9bMuI4m43IWrzgLycQ9UaskUn372ZjHP9InPqHMJU6GQ7vZUe9Tgza\n"
  "qnBdRPoxnrZzUOzlvatGrePt0hDiOZaMtDAkeEojFp9Wp2ZN7+tZAgMBAAE=\n"
  "-----END RSA PUBLIC KEY-----\n"
  ;

