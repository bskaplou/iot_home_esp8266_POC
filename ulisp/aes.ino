#include <stdio.h>
#include <AES.h>

#define AES_KEYSIZE 128

uint8_t char2int(char input) {
  if(input >= '0' && input <= '9')
    return input - '0';
  if(input >= 'A' && input <= 'F')
    return input - 'A' + 10;
  if(input >= 'a' && input <= 'f')
    return input - 'a' + 10;
  return 0;
}

void hex2bin(const char* src, uint8_t* target) {
  while(*src && src[1]) {
    *(target++) = char2int(*src)*16 + char2int(src[1]);
    src += 2;
  }
}

AES aes;

uint8_t* aes_enc(const char* message, ssize_t *len) {
  uint8_t iv[16];
  for(int i = 0; i < 16; i++) {
    iv[i] = (uint8_t) random(0, 255);
  }

  ssize_t mlen = strlen(message);
  ssize_t pad_len = (mlen / 16 + ((mlen % 16) ? 1: 0)) * 16;
  uint8_t* pad_message = (uint8_t*) malloc(pad_len);
  memset(pad_message, 0, pad_len);
  memcpy(pad_message, message, mlen + 1);

  *len = pad_len + 16;
  uint8_t* cypher = (uint8_t*) malloc(*len);
  memcpy(cypher, iv, 16);
  memset(cypher + 16, 0, pad_len);

  aes.cbc_encrypt(pad_message, cypher + 16, pad_len / 16, iv);

  free(pad_message);

  return cypher; //let it free later
}

char* aes_dec(uint8_t* cypher, ssize_t len) {
  if(len < 20) {
    return NULL;
  }

  uint8_t iv[16];
  memcpy(iv, cypher, sizeof(iv));

  uint8_t message_len = len + 16 + 1;
  unsigned char* message = (unsigned char*) malloc(message_len);
  memset(message, 0, message_len);

  if(SUCCESS != aes.cbc_decrypt(cypher + 16, message, (len / 16) - 1, iv)) {
    Serial.println("Decrypt fuckup");
    return NULL;
  }

  for(uint8_t idx = 0; idx < message_len; idx++) {
    if(! isascii(message[idx])
       && message[idx] != '\0'
       && message[idx] != 0xa8
       && message[idx] != 0xb8
       && !(message[idx] >= 0xc0 && message[idx] <= 0xff)
    ) {
      free(message);
      return NULL;
    }
  }

  //Serial.println((char*) message);
  return (char*) message; //let it free later
}

uint8_t aes_setup(const char* hex_key) {
  uint8_t key[AES_KEYSIZE / 8];
  hex2bin(hex_key, key);
  aes.set_key(key, AES_KEYSIZE);
  return 0;
}
