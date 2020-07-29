from Cryptodome.Cipher import AES
from Cryptodome.Random import get_random_bytes
from binascii import hexlify, unhexlify
import sexpdata

import asyncio
import asyncio_dgram

import socket
import logging
from homeassistant.const import TEMP_CELSIUS, CONCENTRATION_PARTS_PER_MILLION, UNIT_PERCENTAGE, DEVICE_CLASS_HUMIDITY, DEVICE_CLASS_ILLUMINANCE, DEVICE_CLASS_TEMPERATURE
from homeassistant.helpers.entity import Entity

from . import DOMAIN

logger = logging.getLogger(__name__)

def encrypt(data, key):
  iv = ''.join('{:02x}'.format(x) for x in get_random_bytes(8)).encode('utf8')
  aes = AES.new(key, AES.MODE_CBC, iv)
  padded_size = len(data) + (16 - len(data) % 16)
  message = iv + aes.encrypt(data.ljust(padded_size, '\x00').encode('utf8'))
  return message

def decrypt(message, key):
  iv = message[0:16]
  aes = AES.new(key, AES.MODE_CBC, iv)
  data = aes.decrypt(message[16:]).decode('utf8').rstrip('\x00')
  return data

async def async_setup_platform(hass, config, add_entities, discovery_info=None):
  """Set up the sensor platform."""
  entities = []
  for k, v in config.items():
    if k == 'hosts':
      for h in v:
        host = {}
        for p, r in h.items():
          host[p] = r

        supported = []
        try:
          key = unhexlify(host['key'])
          cypher = encrypt('(discovery)\0', key)
          sock = await asyncio.wait_for(asyncio_dgram.connect((host['ip'], 54321)), 5)
          await asyncio.wait_for(sock.send(cypher), 5)
          cypher, [source_host, port] = await asyncio.wait_for(sock.recv(), 5)
          message = decrypt(cypher, key)
          logger.info("discover -> %s:%s -> %s", host['ip'], 54321, message)
          supported = sexpdata.loads(message)
        except:
          logger.exception("Failed to init device %s", host['ip'])

        if 'temperature-read' in supported:
          entities.append(HHGSensor("hhg_" + host['ip'] + '_temperature', host['ip'], host['key'], '(temperature-read)', TEMP_CELSIUS, DEVICE_CLASS_TEMPERATURE))

        if 'humidity-read' in supported:
          entities.append(HHGSensor("hhg_" + host['ip'] + '_humidity', host['ip'], host['key'], '(humidity-read)', UNIT_PERCENTAGE, DEVICE_CLASS_HUMIDITY))

        if 'light-read' in supported:
          entities.append(HHGSensor("hhg_" + host['ip'] + '_light', host['ip'], host['key'], '(light-read)', 'lx', DEVICE_CLASS_ILLUMINANCE))

        if 'co2-read' in supported:
          entities.append(HHGSensor("hhg_" + host['ip'] + '_co2', host['ip'], host['key'], '(co2-read)', CONCENTRATION_PARTS_PER_MILLION, 'air_quality'))

        if 'tvoc-read' in supported:
          entities.append(HHGSensor("hhg_" + host['ip'] + '_tvoc', host['ip'], host['key'], '(tvoc-read)', CONCENTRATION_PARTS_PER_MILLION, 'air_quality'))
        
  logger.info(entities)
  add_entities(entities)

class HHGSensor(Entity):
  def __init__(self, name, ip, key, getter, unit, dc):
    self._name = name
    self._ip = ip
    self._key = unhexlify(key)
    self._getter = getter
    self._unit = unit
    self._state = 0
    self._dc = dc

  @property
  def name(self):
    return self._name

  @property
  def state(self):
    return self._state

  @property
  def unit_of_measurement(self):
    return self._unit 

  @property
  def device_class(self):
    return self._dc

  async def async_update(self):
    logger.info("updating " + self._name)
    cypher = encrypt(self._getter + '\0', self._key)

    try:
      sock = await asyncio.wait_for(asyncio_dgram.connect((self._ip, 54321)), 5)
      await asyncio.wait_for(sock.send(cypher), 5)
      cypher, [host, port] = await asyncio.wait_for(sock.recv(), 5)
      message = decrypt(cypher, self._key)
      logger.info("%s -> %s:%s -> %s", self._getter, self._ip, 54321, message)
      self._state = float(message)
    except asyncio.TimeoutError:
      logger.error("Timeout error %s", self._getter)
    except:
      logger.error("Some other arror")
    finally:
      sock.close()

