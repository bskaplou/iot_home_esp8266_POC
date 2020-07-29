import logging
import socket
from functools import reduce
from Cryptodome.Cipher import AES
from Cryptodome.Random import get_random_bytes
import argparse
from base64 import b64decode, b64encode
from binascii import hexlify, unhexlify

_LOGGER = logging.getLogger(__name__)
logging.basicConfig(level="DEBUG")

parser = argparse.ArgumentParser(description='Send command to BCD devices.')
parser.add_argument('--port', type=int, default=54321, help='destination port')
parser.add_argument('--message', type=str, help='lua message', required=True)
parser.add_argument('--key', type=str, help='encryption key', required=False)
args = parser.parse_args()

def encrypt(data):
  iv = ''.join('{:02x}'.format(x) for x in get_random_bytes(8)).encode('utf8')
  aes = AES.new(key, AES.MODE_CBC, iv)
  padded_size = len(args.message) + (16 - len(args.message) % 16)
  message = iv + aes.encrypt(args.message.ljust(padded_size, '\x00').encode('cp1251'))
  return message

def decrypt(message):
  iv = message[0:16]
  aes = AES.new(key, AES.MODE_CBC, iv)
  data = aes.decrypt(message[16:]).decode('utf8').rstrip('\x00')
  return data

if args.key:
  key = unhexlify(args.key)
  response = encrypt(args.message)
else:
  response = args.message.encode('utf8')

sock = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
sock.bind(("0.0.0.0",54321))

message, [host, port] = sock.recvfrom(4096)
if args.key:
  print(decrypt(response))
else:
  print(message.decode('utf8'))

print("Response len", len(response))
sock.sendto(response, (host, port))
