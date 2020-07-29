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
parser.add_argument('--ip', type=str, help='destination address', required=True)
parser.add_argument('--port', type=int, default=54321, help='destination port')
parser.add_argument('--message', type=str, help='ulisp programm', required=False)
parser.add_argument('--mfile', type=str, help='ulisp programm file', required=False)
parser.add_argument('--key', type=str, help='encryption key', required=False)
args = parser.parse_args()

def encrypt(data, key):
  iv = ''.join('{:02x}'.format(x) for x in get_random_bytes(8)).encode('utf8')
  aes = AES.new(key, AES.MODE_CBC, iv)
  padded_size = len(data) + (16 - len(data) % 16)
  message = iv + aes.encrypt(data.ljust(padded_size, '\x00').encode('cp1251'))
  return message

def decrypt(message, key):
  iv = message[0:16]
  aes = AES.new(key, AES.MODE_CBC, iv)
  data = aes.decrypt(message[16:]).decode('utf8').rstrip('\x00')
  return data

def send(s, key, ip, port, msg):
  if key:
    message = encrypt(msg, key)
  else:
    message = msg.encode('utf8')

  #print("Message len", len(message))
  s.sendto(message, (ip, port))

def recv(s, key):
  message, [host, port] = s.recvfrom(4096)
  if key:
    return decrypt(message, key)
  else:
    return message.decode('utf8')

def bracket_group(mess):
  tor = []
  depth = 0
  last = 0
  for i in range(len(mess)):
    #print(i, ">", mess[i])
    if mess[i] == '(':
      if depth == 0:
        last = i
      depth += 1
    elif mess[i] == ')':
      depth -= 1
      if depth == 0:
        #print(mess[last:(i + 1)])
        tor.append(mess[last:(i + 1)])

  if depth != 0:
    return None # Unbalanced!

  return tor

sock = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
sock.settimeout(5)

if args.message:
  msg = bracket_group(args.message)
elif args.mfile:
  with open(args.mfile, 'r') as file:
    content = file.read()

  msg = bracket_group(content)

if args.key:
  binkey = unhexlify(args.key)
else:
  binkey = None

for exp in msg:
  print(">>", exp)
  send(sock, binkey, args.ip, args.port, exp)
  print("<<",recv(sock, binkey))
