/* Copyright 2001-2004 Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/* Our unit test expect that the AUTHORITY_CERT_* public keys will sort
 * in this order. */
#define AUTHORITY_CERT_A AUTHORITY_CERT_3
#define AUTHORITY_CERT_B AUTHORITY_CERT_1
#define AUTHORITY_CERT_C AUTHORITY_CERT_2

#define AUTHORITY_SIGNKEY_A AUTHORITY_SIGNKEY_3
#define AUTHORITY_SIGNKEY_B AUTHORITY_SIGNKEY_1
#define AUTHORITY_SIGNKEY_C AUTHORITY_SIGNKEY_2

/** First of 3 example authority certificates for unit testing. */
const char AUTHORITY_CERT_A[] =
"dir-key-certificate-version 3\n"
"fingerprint D867ACF56A9D229B35C25F0090BC9867E906BE69\n"
"dir-key-published 2008-12-12 18:07:24\n"
"dir-key-expires 2009-12-12 18:07:24\n"
"dir-identity-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIIBigKCAYEAveMpKlw8oD1YqFqpJchuwSR82BDhutbqgHiez3QO9FmzOctJpV+Y\n"
"mpTYIJLS/qC+4GBKFF1VK0C4SoBrS3zri0qdXdE+vBGcyrxrjMklpxoqSKRY2011\n"
"4eqYPghKlo5RzuqteBclGCHyNxWjUJeRKDWgvh+U/gr2uYM6fRm5q0fCzg4aECE7\n"
"VP6fDGZrMbQI8jHpiMSoC9gkUASNEa6chLInlnP8/H5qUEW4TB9CN/q095pefuwL\n"
"P+F+1Nz5hnM7fa5XmeMB8iM4RriUmOQlLBZgpQBMpEfWMIPcR9F1Gh3MxERqqUcH\n"
"tmij+IZdeXg9OkCXykcabaYIhZD3meErn9Tax4oA/THduLfgli9zM0ExwzH1OooN\n"
"L8rIcJ+2eBo3bQiQUbdYW71sl9w7nSPtircbJUa1mUvWYLPWQxFliPiQSetgJLMj\n"
"VQqtPmV2hvN2Xk3lLfJO50qMTK7w7Gsaw8UtV4YDM1Hcjp/hQaIB1xfwhXgl+eUU\n"
"btUa4c+cUTjHAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"dir-signing-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIGJAoGBALPSUInyuEu6NV3NjozplaniIEBzQXEjv1x9/+mqnwZABpYVmuy9A8nx\n"
"eoyY3sZFsnYwNW/IZjAgG23pEmevu3F+L4myMjjaa6ORl3MgRYQ4gmuFqpefrGdm\n"
"ywRCleh2JerkQ4VxOuq10dn/abITzLyaZzMw30KXWp5pxKXOLtxFAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"dir-key-crosscert\n"
"-----BEGIN ID SIGNATURE-----\n"
"FTBJNR/Hlt4T53yUMp1r/QCSMCpkHJCbYBT0R0pvYqhqFfYN5qHRSICRXaFFImIF\n"
"0DGWmwRza6DxPKNzkm5/b7I0de9zJW1jNNdQAQK5xppAtQcAafRdu8cBonnmh9KX\n"
"k1NrAK/X00FYywju3yl/SxCn1GddVNkHYexEudmJMPM=\n"
"-----END ID SIGNATURE-----\n"
"dir-key-certification\n"
"-----BEGIN SIGNATURE-----\n"
"pjWguLFBfELZDc6DywL6Do21SCl7LcutfpM92MEn4WYeSNcTXNR6lRX7reOEJk4e\n"
"NwEaMt+Hl7slgeR5wjnW3OmMmRPZK9bquNWbfD+sAOV9bRFZTpXIdleAQFPlwvMF\n"
"z/Gzwspzn4i2Yh6hySShrctMmW8YL3OM8LsBXzBhp/rG2uHlsxmIsc13DA6HWt61\n"
"ffY72uNE6KckDGsQ4wPGP9q69y6g+X+TNio1KPbsILbePv6EjbO+rS8FiS4njPlg\n"
"SPYry1RaUvxzxTkswIzdE1tjJrUiqpbWlTGxrH9N4OszoLm45Pc784KLULrjKIoi\n"
"Q+vRsGrcMBAa+kDowWU6H1ryKR7KOhzRTcf2uqLE/W3ezaRwmOG+ETmoVFwbhk2X\n"
"OlbXEM9fWP+INvFkr6Z93VYL2jGkCjV7e3xXmre/Lb92fUcYi6t5dwzfV8gJnIoG\n"
"eCHd0K8NrQK0ipVk/7zcPDKOPeo9Y5aj/f6X/pDHtb+Dd5sT+l82G/Tqy4DIYUYR\n"
"-----END SIGNATURE-----\n";

/** The private signing key for AUTHORITY_CERT_1 */
const char AUTHORITY_SIGNKEY_A[] =
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIICWwIBAAKBgQCz0lCJ8rhLujVdzY6M6ZWp4iBAc0FxI79cff/pqp8GQAaWFZrs\n"
"vQPJ8XqMmN7GRbJ2MDVvyGYwIBtt6RJnr7txfi+JsjI42mujkZdzIEWEOIJrhaqX\n"
"n6xnZssEQpXodiXq5EOFcTrqtdHZ/2myE8y8mmczMN9Cl1qeacSlzi7cRQIDAQAB\n"
"AoGASpzUkDinIbzU0eQt5ugxEnliOnvYRpK3nzAk1JbYPyan1PSIAPz4qn1JBTeV\n"
"EB3xS7r7ITO8uvFHkFZqLZ2sH1uE6e4sAytJGO+kyqnlkiDTPEXpcGe99j8PH1yj\n"
"xUOrHRlAYWjG8NEkQi+APA+HZkswE3L/viFwR2AARoE2ac0CQQDsOLdNJa+mqn6N\n"
"1L76nEl/YgXHtKUks+beOR3IgknKEjcsJJEUHyiu0wjbXZV6gTtyQvcAePglUUD1\n"
"R2OkOOADAkEAwuCxvHEAPeQbVt8fSvxw74vqew6LITP2Utb1dQK0E26IRPF36BsJ\n"
"buO/gqMZv6ALq+/KxpA/pUsApbgog9uUFwJAYvHCvbrKX1pM1iXFtP1fv86UMzlU\n"
"bxI34t8zvXftZonIuGG8rxv6E3hr3k7NvNmCx/KKuZTyA9eMCPFVKEV2dwJACn8j\n"
"06yagLrqphE6lEVop953cM1lvRIZcHjXm8fbfzhy6pO/C6d5KJnn1NeIKYQrXMV7\n"
"vJpEc1jI3iQ/Omr3XQJAEBIt5MlP2wlrX9om7B+32XBygUssY3cw/bXybZrtSU0/\n"
"Yx4lqK0ca5IkTp3HevwnlWaJgbaOTUspCVshzJBhDA==\n"
"-----END RSA PRIVATE KEY-----\n";

const char AUTHORITY_SIGNKEY_A_DIGEST[] =
  "CBF56A83368A5150F1A9AAADAFB4D77F8C4170E2";
const char AUTHORITY_SIGNKEY_A_DIGEST256[] =
  "AF7C5468DBE3BA54A052726038D7F15F3C4CA511B1952645B3D96D83A8DFB51C";

/** Second of 3 example authority certificates for unit testing. */
const char AUTHORITY_CERT_B[] =
"dir-key-certificate-version 3\n"
"fingerprint AD011E25302925A9D39A80E0E32576442E956467\n"
"dir-key-published 2013-11-14 14:12:05\n"
"dir-key-expires 2014-11-14 14:12:05\n"
"dir-identity-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIIBigKCAYEAyXYEMlGNRAixXdg65xf2WPkskYj2Wo8ysKMTls1JCXdIOAPvC2k2\n"
"+AC6i3x9JHzUgCjWr4Jd5PSi7ODGyFC543igYl4wzkxNTU2L+SQ+hMe9qbEuUNhH\n"
"sRR0xofdoH//3UuKj+HXEiMhhHbRWQGtWFuJqtGBruJqjZqIGOrp5nFjdlP0R98n\n"
"Rx5wWlPgdJzifkXjKouu4mV+KzLl7f0gAtngA9DkSjt1wzga5IlL/lxDciD0SyJU\n"
"tKMmls056omrZNbTnBxnY2pOlq9nx/zFrt/KQm1fTAQMjMBCf9KnDIV7NhaaHx7F\n"
"7Nk8L7Hha353SvR+bsOFpiu05/EMZFTTIhO3MhUxZiCVZ0hKXvW1xe0HoGC5wbB+\n"
"NyXu8oa4fIKLJ+WJ8Z60BNc0DcxJiQOf1eolGM/qrBul1lFZznds5/7182d+nF2W\n"
"+bEjSm0fgXIxPfSD/7hB0FvgtmB3TXybHGBfPZgX0sTzFB6LNtP0BHicRoMXKdLF\n"
"hM3tgIjEAsoZAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"dir-signing-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIGJAoGBAJ567PZIGG/mYWEY4szYi/C5XXvf0BkquzKTHKrqVjysZEys9giz56Gv\n"
"B08kIRxsxYKEWkq60rv0xtTc1WyEMcDpV1WLU0KSTQSVXzLu7BT8jbTsWzGsxdTV\n"
"TdeyOirwHh8Cyyon5lppuMH5twUHrL5O7pWWbxjjrQjAHCn3gd+NAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"dir-key-crosscert\n"
"-----BEGIN ID SIGNATURE-----\n"
"OC+gaukd4K7xJOsgTPbRhacf5mDUGxsu3ho/J1oJdtni4CK9WscVs6/Goj1o5Lot\n"
"H1nCAMaR96Jnqq5c63Aaj1sEXdeYHlu5cI7YHgtGI5MmtjiUNXUCWMjCwSQYwGKe\n"
"2YDYGAKAGt97n7XMKhJWGjAmv1TgmK3DvL1jt/aazL8=\n"
"-----END ID SIGNATURE-----\n"
"dir-key-certification\n"
"-----BEGIN SIGNATURE-----\n"
"BddmCKsvS6VoFXIf9Aj9OZnfyVCx527517QtsQHN+NaVm20LzUkJ5MWGXYx4wgh3\n"
"ExsHvVQguiVfnonkQpEHHKg+TbldlkuDhIdlb9f7dL7V3HLCsEdmS1c3A+TEyrPH\n"
"i44p6QB5IMFAdgUMV/9ueKMh7pMoam6VNakMOd+Axx9BSJTrCRzcepjtM4Z0cPsj\n"
"nmDgZi0df1+ca1t+HnuWyt3trxlqoUxRcPZKz28kEFDJsgnRNvoHrIvNTuy9qY4x\n"
"rONnPuLr5kTO7VQVVZxgxt6WX3p6d8tj+WYHubydr2pG0dwu2vGDTy4qXvDIm/I4\n"
"Gyo6OAoPbYV8fl0584EgiEbAWcX/Pze8mXr9lmXbf73xbSBHqveAs0UfB+4sBI98\n"
"v4ax4NZkGs8cCIfugtAOLgZE0WCh/TQYnQ3PFcrUtj0RW+tM1z7S8P3UfEVBHVkJ\n"
"8SqSB+pbsY6PwMuy6TC3WujW7gmjVanbwkbW19El9l9jRzteFerz7grG/WQkshqF\n"
  "-----END SIGNATURE-----\n";

/** The private signing key for AUTHORITY_CERT_2 */
const char AUTHORITY_SIGNKEY_B[] =
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIICWwIBAAKBgQCeeuz2SBhv5mFhGOLM2IvwuV1739AZKrsykxyq6lY8rGRMrPYI\n"
"s+ehrwdPJCEcbMWChFpKutK79MbU3NVshDHA6VdVi1NCkk0ElV8y7uwU/I207Fsx\n"
"rMXU1U3Xsjoq8B4fAssqJ+ZaabjB+bcFB6y+Tu6Vlm8Y460IwBwp94HfjQIDAQAB\n"
"AoGAfHQ4ZmfTmPyoeGHcqdVcgBxxh3gJqdnezCavGqGQO3F+CqDBTbBKNLSI3uOW\n"
"hQX+TTK23Xy9RRFCm6MYj3F4x7OOrSHSFyhMmzRnAZi3zGbtQZn30XoqTwCmVevY\n"
"p5JbVvhP2BJcvdsyQhiIG23FRQ7MMHWtksAxmovTto1h/hkCQQDNCfMqSztgJZDn\n"
"JSf5ASHBOw8QzfZBeYi3hqfiDtAN1RxT1uQnEiFQFJqwCz5lCbcwVrfQbrrk5M+h\n"
"ooYrX7tTAkEAxd6Tl0N0WM3zCKz+3/Hoiyty6olnnpzNoPCg7LLBJcetABQi0KUv\n"
"swYWlKP3eOFZkiBzTqa9nBK7eYLKV3d9nwJAKNM3WI98Nguky3FJgTnpd6kDuevY\n"
"gXbqcuhb2xXp9Sceqc7axLDGc0R2/GBwvvttPzG1DcpOai7o7J0Iq/A2wwJAYuKI\n"
"/99GFdtWyc8q0OAkRui/1VY14p6aZQPcaG4s+KSBYLivbXYgEGfKgR4wXsi/6rcs\n"
"6PGLcKQr7N3gITYmIQJAaQn6djUWygCn1noKyWU+Sa7G5qqU2GWkLq9dMaRLm1/I\n"
"nqi+2K1mN15rra0QtFVqSH4JXr8h3KAGyU45voGM7A==\n"
"-----END RSA PRIVATE KEY-----\n";

/** Third of 3 example authority certificates for unit testing. */
const char AUTHORITY_CERT_C[] =
"dir-key-certificate-version 3\n"
"fingerprint 628C2086EC29C9D26E638C5A8B2065BFBD35829B\n"
"dir-key-published 2013-11-14 14:12:18\n"
"dir-key-expires 2014-11-14 14:12:18\n"
"dir-identity-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIIBigKCAYEAuzPA82lRVUAc1uZgfDehhK0rBU5xt+qhJXUSH0DxsuocYCLW//q+\n"
"7+L7q9SochqZK3R5+SxJaZRlVK4rAeIHsxXFxsnGvuqasGM3he80EV1RpVRkvLaO\n"
"2dDmHcfEjYBadft2DEq811yvqSRqbFXmK0hLucA6LI6NnEw9VNWlguaV6ACVLyKQ\n"
"iYVFz2JOJIAi0Zz57WZg7eHypUAGoyXjtYTJPsh6pUe/0NLFJVd3JHcJX+bNqU2a\n"
"QU37r+CQ9f3T+8fZGJQ/CXNnYUNHa0j+toOFuPEiZBBh8C4PE7FJWjidvhe9uI7T\n"
"Py41RZhy8e05MAQmUBNRKBHWPKHoy2zWZZxTkcfWFdJJz/dzsNrIjrqf2fYId9To\n"
"fDpHzYd/UjzZaaVYRVS/Oyf3pN8DKw8LMhEArS0X9pblPVkWWjmYMU6f0VR7pelc\n"
"gGYuML3gOiKdNbeMWgAv3HNRsVsuW0HZLrhXUGYzTRPJ/GxVCwA/NmYgMTNVWRwF\n"
"7M78YHpayyEPAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"dir-signing-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIGJAoGBANESf/hRRWCK3TLQyNb9Y42tYedCORUc8Rl+Q4wrvdz3R0TNr6rztE9N\n"
"u8v3Wbvjtiqm1xL1I5PaOObFQQj61QZxKiCm1yU4eFH15dNmcvBEy5BjEXVYiDgy\n"
"zKRyePzjHYQIZF3ZaQTABUplkXVpY0YvAurluhEy+dKEvZMwWFZTAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"dir-key-crosscert\n"
"-----BEGIN ID SIGNATURE-----\n"
"NHNBya6Dt7Ww3qSGA0DBEl6pZFBzmYXM+QdqF+ESpdyYCQ54EYimaxl4VcXoGaxy\n"
"xk8/VOXPC6h7hVnTWDTsC86G6eXug1yzpd/uhQbcDJMH5q8/Yg5WXGOnGhMWNCBh\n"
"u2UmbtAjdjLrObQaB50FfOpuOV9kdG4SEzaPUBR2ayU=\n"
"-----END ID SIGNATURE-----\n"
"dir-key-certification\n"
"-----BEGIN SIGNATURE-----\n"
"NocTkLl9iKglVo+yrpY0slsqgPviuScMyEfOJ3i65KeJb4Dr1huIs0Fip40zFD8D\n"
"cz/SYu09FbANuRwBJIRdVWZLLwVFLBj5F8U65iJRAPBw/O/xgSVBvWoOhBUZqmJA\n"
"Jp1IUutQHYFfnAOO9za4r8Ox6yPaOWF9Ks5gL0kU/fI8Bdi5E9p3e9fMtoM7hROg\n"
"oX1AoV/za3LcM0oMsGsdXQ7B8vRqY0eUX523kpRpF1fUDyvBUvvMsXdZDN6anCV6\n"
"NtSq2UaM/msTX1oQ8gzyD1gMXH0Ek26YMhd+6WZE6KUeb1x5HJgXtKtYzMLB6nQM\n"
"4Q/OA4NND/Veflofy6xx8uzXe8H+MoUHK9WiORtwqvBl0E9qk6SVCuo4ipR4Ybgk\n"
"PAFOXA58j80dlNYYEVgV8MXF1Y/g/thuXlf2dWiLAExdHTtE0AzC4quWshegaImC\n"
"4aziHeA43TRDszAXcJorREAM0AhSxp3aWDde4Jt46ODOJR8t+gHreks29eDttEIn\n"
"-----END SIGNATURE-----\n";

/** The private signing key for AUTHORITY_CERT_3 */
const char AUTHORITY_SIGNKEY_C[] =
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIICXAIBAAKBgQDREn/4UUVgit0y0MjW/WONrWHnQjkVHPEZfkOMK73c90dEza+q\n"
"87RPTbvL91m747YqptcS9SOT2jjmxUEI+tUGcSogptclOHhR9eXTZnLwRMuQYxF1\n"
"WIg4Msykcnj84x2ECGRd2WkEwAVKZZF1aWNGLwLq5boRMvnShL2TMFhWUwIDAQAB\n"
"AoGAU68L+eDN3C65CzX2rdcOmg7kOSSQpJrJBmM7tkdr3546sJeD0PFrIrMCkEmZ\n"
"aVNj/v545+WnL+8RB4280lNUIF4AMNaMZUL+4FAtwekqWua3QvvqgRMjCdG3/h/d\n"
"bOAUiiKKEimflTaIVHNVSCvOIntftOu3PhebctuabnZzg0ECQQD9i+FX7M9UXT1A\n"
"bVm+bRIJuQtG+u9jD3VxrvHsmh0QnOAL3oa/ofTCwoTJLZs8Qy0GeAoJNf28rY1q\n"
"AgNMEeEXAkEA0xhxNX2fDQ2yvKwPkPMrRycJVWry+KHvSZG2+XYh+V5sVGQ5H7Gu\n"
"krc6IzRZlIKQhEGktkw8ih0DEHQbAihiJQJBAKi/SnFcePjrPXL91Hb63MB/2dOZ\n"
"+21wwnexOe6A+8ssvajop8IvJlnhYMMMiX7oLrVZe0R6HLBQyge94zfjxm0CQGye\n"
"dRIrE34qAEBo4JGbLjesdHcJUwBwgqn+WoI+MPkZhvBdqa8PRF6l/TpEI5vxGt+S\n"
"z2gmDjia+QqsU4FmuikCQDDOs85uwNSKJFax9XMzd1qd1QwX20F8lvnOsWErXiDw\n"
"Fy2+rmIRHoSxn4D+rE5ivqkO99E9jAlz+uuQz/6WqwE=\n"
"-----END RSA PRIVATE KEY-----\n";

