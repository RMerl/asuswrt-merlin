# This is the curve25519 implementation from Matthew Dempsky's "Slownacl"
# library.  It is in the public domain.
#
# It isn't constant-time.  Don't use it except for testing.
#
# Nick got the slownacl source from:
#        https://github.com/mdempsky/dnscurve/tree/master/slownacl

__all__ = ['smult_curve25519_base', 'smult_curve25519']

import sys

P = 2 ** 255 - 19
A = 486662

def expmod(b, e, m):
  if e == 0: return 1
  t = expmod(b, e // 2, m) ** 2 % m
  if e & 1: t = (t * b) % m
  return t

def inv(x):
  return expmod(x, P - 2, P)

# Addition and doubling formulas taken from Appendix D of "Curve25519:
# new Diffie-Hellman speed records".

def add(n,m,d):
  (xn,zn), (xm,zm), (xd,zd) = n, m, d
  x = 4 * (xm * xn - zm * zn) ** 2 * zd
  z = 4 * (xm * zn - zm * xn) ** 2 * xd
  return (x % P, z % P)

def double(n):
  (xn,zn) = n
  x = (xn ** 2 - zn ** 2) ** 2
  z = 4 * xn * zn * (xn ** 2 + A * xn * zn + zn ** 2)
  return (x % P, z % P)

def curve25519(n, base):
  one = (base,1)
  two = double(one)
  # f(m) evaluates to a tuple containing the mth multiple and the
  # (m+1)th multiple of base.
  def f(m):
    if m == 1: return (one, two)
    (pm, pm1) = f(m // 2)
    if (m & 1):
      return (add(pm, pm1, one), double(pm1))
    return (double(pm), add(pm, pm1, one))
  ((x,z), _) = f(n)
  return (x * inv(z)) % P

if sys.version < '3':
    def b2i(c):
        return ord(c)
    def i2b(i):
        return chr(i)
    def ba2bs(ba):
        return "".join(ba)
else:
    def b2i(c):
        return c
    def i2b(i):
        return i
    def ba2bs(ba):
        return bytes(ba)

def unpack(s):
  if len(s) != 32: raise ValueError('Invalid Curve25519 argument')
  return sum(b2i(s[i]) << (8 * i) for i in range(32))

def pack(n):
  return ba2bs([i2b((n >> (8 * i)) & 255) for i in range(32)])

def clamp(n):
  n &= ~7
  n &= ~(128 << 8 * 31)
  n |= 64 << 8 * 31
  return n

def smult_curve25519(n, p):
  n = clamp(unpack(n))
  p = unpack(p)
  return pack(curve25519(n, p))

def smult_curve25519_base(n):
  n = clamp(unpack(n))
  return pack(curve25519(n, 9))


#
# This part I'm adding in for compatibility with the curve25519 python
# module. -Nick
#
import os

class Private:
    def __init__(self, secret=None, seed=None):
        self.private = pack(clamp(unpack(os.urandom(32))))

    def get_public(self):
        return Public(smult_curve25519_base(self.private))

    def get_shared_key(self, public, hashfn):
        return hashfn(smult_curve25519(self.private, public.public))

    def serialize(self):
        return self.private

class Public:
    def __init__(self, public):
        self.public = public

    def serialize(self):
        return self.public

