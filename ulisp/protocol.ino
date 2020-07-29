#include <WiFiUdp.h>

// Communication protocol

#define UDP_PORT 54321

WiFiUDP  udp;

char     pending_host[64] = "";
uint16_t pending_port = 0;
char*    pending_callback = NULL;

char* ulisp_request(const char* host, uint16_t port, const char* message, char* callback) {
  memcpy(pending_host, host, strlen(host) + 1);

  pending_port = port;

  if(pending_callback != NULL) {
    free(pending_callback);
  }
  pending_callback = callback;

  ssize_t cypher_len;
  uint8_t* cypher = aes_enc(message, &cypher_len);

  udp.beginPacket(host, port);
  udp.write(cypher, cypher_len);
  udp.endPacket();

  free(cypher);
}

void udp_loop_crypto() {
  uint32_t size = udp.parsePacket();
  if (size) {
    Serial.printf("Received %d bytes from %s, port %d\n", size, udp.remoteIP().toString().c_str(), udp.remotePort());
    bool response_message = (udp.remotePort() == pending_port 
      && 0 == strcmp(udp.remoteIP().toString().c_str(), pending_host));

    uint8_t* buff = (uint8_t*) malloc(size);
    int len = udp.read(buff, size);
    char* message = aes_dec(buff, len);
    //Serial.println("after dec");

    if(message == NULL) {
      Serial.printf("Failed to decrypt UDP packet ;(\n");
    } else if(response_message){
      pending_port = 0;
      pending_host[0] = 0;
      Serial.printf("response UDP packet decrypted content: %s\n", message);
      if(pending_callback != NULL) {
        char code[256];
        sprintf(code, "(%s '%s)", pending_callback, message);
        Serial.printf("calling back: %s\n", code);
        ulisp_run(code);
        free(pending_callback);
        pending_callback = NULL;
      }
      free(message);
    } else {
      //Serial.print("Message: ");
      //Serial.print(message);
      //Serial.print(" size: ");
      //Serial.println(size);
      Serial.printf("request UDP packet decrypted content: %s\n", message);

      ssize_t cypher_len;
      char* result = ulisp_run(message);
      uint8_t* cypher = aes_enc(result, &cypher_len);
      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      udp.write(cypher, cypher_len);
      udp.endPacket();
      free(cypher);
      free(message);
    }
    free(buff);
  }
}

void udp_loop_plain() {
  uint32_t size = udp.parsePacket();
  if (size) {
    Serial.printf("Received %d bytes from %s, port %d\n", size, udp.remoteIP().toString().c_str(), udp.remotePort());

    char* message = (char*) malloc(size + 1);
    int len = udp.read(message, size);
    message[size] = '\0'; // clang trailing zero is not necessary, we add it on server side

    Serial.printf("UDP packet contents: %s\n", message);

    char* response = (char*) ulisp_run(message);
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(response, strlen(response) + 1);
    udp.endPacket();

    free(message);
  }
}

void udp_setup() {
  Serial.print("Starting UDP server... ");
  Serial.println(udp.begin(UDP_PORT) == 1 ? "success" : "failure");
  udp_loop = use_crypto ? udp_loop_crypto : udp_loop_plain;
}
