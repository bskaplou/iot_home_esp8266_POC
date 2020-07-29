// Header necessary for any setup

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <ESP8266mDNS.h>
#include <stdio.h>
#include "ArduinoJson.h"

// Devices declaration

// Single color led panel Max72xx
#define USEMAX72
#ifdef  USEMAX72
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>
#include <Fonts/Picopixel.h>
#include <Fonts/TomThumb.h>
#define CLK_PIN   14
#define DATA_PIN  13
#define CS_PIN    16
Max72xxPanel  matrix = Max72xxPanel(CS_PIN, 4, 1);
char*         matrix_message = NULL;
uint32_t      matrix_message_len = 0;
unsigned long matrix_message_stoptime = 0;
unsigned long matrix_message_starttime = 0;
uint32_t      matrix_message_width = 0;
uint32_t      matrix_message_height = 0;
uint8_t       matrix_font_shift = 0; // Y
uint8_t       matrix_speed = 50; // ms/pixel
long          matrix_stop_cursor = 0;
#endif

// Humidity/temperature sensor
//#define USEHDC1080
#ifdef  USEHDC1080
#include <ClosedCube_HDC1080.h>
ClosedCube_HDC1080 hdc1080;
#endif

// Air quality sensor ccs811 co2/tvoc
//#define USECCS811
#ifdef  USECCS811
#define CCS811_ADDR 0x5B
#include <Adafruit_CCS811.h>
Adafruit_CCS811 ccs;
#endif

// Digital light sensor
//#define USEBH1750
#ifdef  USEBH1750
#include <BH1750FVI.h>
BH1750FVI lux(0x23);
#endif

// Analog light sensor
//#define USETEMP6000
#ifdef USETEMP6000
#define ADC_PIN 17
#endif

// Distance sensor
//#define USEVL53L0X
#ifdef  USEVL53L0X
#include <VL53L0X.h>
VL53L0X distance;
#endif

//#define USEIRR
#ifdef  USEIRR
#define IRR_PIN 13
#define IRR_BUFFER_SIZE 1024
#define IRR_TIMEOUT 15
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>
IRrecv irrecv(IRR_PIN, IRR_BUFFER_SIZE, IRR_TIMEOUT, true);
decode_results results;
#endif

//#define USEIRS
#ifdef  USEIRS
#define IRS_PIN 14
#include <IRremoteESP8266.h>
#include <IRsend.h>
IRsend irsend(IRS_PIN);
#endif

//#define USEBUTTON
#ifdef  USEBUTTON
#define BUTTON_PIN 12
#define BUTTON_RESET_DURATION 2000
unsigned long button_push_stamp;
#endif

//#define USELED
#ifdef  USELED
#define LED_PIN 14
#endif

#define UDP_PORT 54321
#define CONFIG_SIZE_MAX 256

void (*udp_loop)();

StaticJsonDocument<CONFIG_SIZE_MAX> cfg;
uint8_t use_crypto = 1;

void mac_to_id(char* dest, const char* mac) {
  for(uint8_t idx = 0, buff_idx = 0; idx < 18; idx++) {
    if((idx + 1) % 3 != 0) {
      dest[buff_idx++] = tolower(mac[idx]);
    }
  }
}

// mDNS
void mdns_setup() {
  char *devname = "hhg_sensor_XXXXXXXXXXXX";
  mac_to_id(devname + 11, WiFi.softAPmacAddress().c_str());
  Serial.printf("mDNS starting... %s ", devname);
  if(! MDNS.begin(devname)) {
    Serial.println("failed");
  } else {
    MDNS.addService("hhg", "udp", UDP_PORT);
    MDNS.addServiceTxt("hhg", "udp", "crypto", use_crypto != 0 ? "true" : "false");
    MDNS.addServiceTxt("hhg", "udp", "mac", WiFi.softAPmacAddress().c_str());
    Serial.println("success");
  }
}

void mdns_loop() {
  MDNS.update();
}

// WiFi Station
void wifi_sta(const char* ssid, const char* pwd) {
  WiFi.enableSTA(true);
  WiFi.persistent(false);
  WiFi.begin(ssid, pwd);

  Serial.printf("Wi-Fi connecting to %s", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("success");

  Serial.print("Wi-Fi connected, IP address: ");
  Serial.println(WiFi.localIP());
}

// WiFi Access point
WiFiEventHandler stationConnectedHandler;
WiFiEventHandler stationDisconnectedHandler;
WiFiEventHandler probeRequestPrintHandler;

void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt) {
  Serial.println("ap -> station connected");
}

void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt) {
  Serial.println("ap -> station disconnected");
}

void wifi_ap() {
  WiFi.printDiag(Serial);
  WiFi.persistent(false);
  WiFi.disconnect(true);

  IPAddress ip(192, 168, 19 , 16);
  IPAddress subnet(255, 255, 255, 0);

  char *ap_name = "lonely_hhg_XXXXXXXXXXXX";
  mac_to_id(ap_name + 11, WiFi.softAPmacAddress().c_str());

  WiFi.enableAP(true);
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(ip, ip, subnet) ? "Ready" : "Failed!");
  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(ap_name, NULL, 11, false, 8) ? "Ready" : "Failed!");
  Serial.print("Started WiFi AP with name: ");
  Serial.print(ap_name);
  Serial.print(" address: ");
  Serial.println(WiFi.softAPIP());

  stationConnectedHandler = WiFi.onSoftAPModeStationConnected(&onStationConnected);
  stationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(&onStationDisconnected);
}

// Permanent storage

const char* config_file = "config";

void fs_setup() {
  SPIFFSConfig cfg;
  cfg.setAutoFormat(true);
  SPIFFS.setConfig(cfg);
  SPIFFS.begin();
}

#define CONFIG_SIZE 256

char* fs_loadconfig() {
  Serial.println("fs_loadconfig");
  char buff[CONFIG_SIZE];
  if(SPIFFS.exists(config_file)) {
    File f = SPIFFS.open(config_file, "r");
    ssize_t in = f.readBytes(buff, CONFIG_SIZE);
    f.close();
    buff[in] = '\0';
  } else {
    return NULL;
  }
  ssize_t sz = strlen(buff) + 1;
  char* tor = (char*) malloc(sz);
  memcpy(tor, buff, sz);
  return tor;
}

uint8_t fs_parseconfig() {
  char* config = fs_loadconfig();
  if(config != NULL && strlen(config) == 0) {
    Serial.println("config not found");
    return 0;
  } else {
    DeserializationError err = deserializeJson(cfg, config);
    if(err != DeserializationError::Ok) {
      Serial.println("config json parsing failed");
      return 0;
    }
  }
  return 1;
}

void fs_saveconfig(const char* data) {
  File f = SPIFFS.open(config_file, "w");
  Serial.print("Saving configration: ");
  Serial.print(data);
  Serial.print(" , bytes written: ");
  size_t in = f.write(data);
  Serial.print(in);
  f.close();
}

void fs_removeconfig() {
  SPIFFS.remove(config_file);
}

// Button

#ifdef USEBUTTON
ICACHE_RAM_ATTR void button_change() {
  uint8_t state = digitalRead(BUTTON_PIN);
  if(state == 1) {
    button_push_stamp = millis();
  } else {
    button_push_stamp = 0;
  }
}
#endif

void button_loop() {
#ifdef USEBUTTON
  if(button_push_stamp != 0) {
    unsigned long duration = millis() - button_push_stamp;
    Serial.print("Push duration: ");
    Serial.println(duration);
    if(duration > BUTTON_RESET_DURATION) {
      fs_removeconfig();
      Serial.println("Config deleted... I'll restart with factory settings right away... Set me up afterwards pls.");
      ESP.restart();
    }
  }
#endif
}

void message_loop() {
#ifdef  USEMAX72
  if(matrix_message != NULL) {
    long matrix_scroll = (millis() - matrix_message_starttime) / matrix_speed;
    static long last_cursor_x = 0;
    long cursor_x =  matrix.width() - matrix_scroll % (matrix_message_width + matrix.width());
    if(cursor_x == last_cursor_x) {
      return;
    }
    last_cursor_x = cursor_x;

    //Serial.print(matrix_stop_cursor);
    //Serial.print(" > ");
    //Serial.println(cursor_x);

    if(millis() >= matrix_message_stoptime) {
      if(matrix_stop_cursor != 0 && matrix_stop_cursor < cursor_x) {
        free(matrix_message);
        matrix_stop_cursor = 0;
        matrix_message = NULL;
        matrix.shutdown(true);
        matrix.fillScreen(LOW);
        matrix.write();
      }
      matrix_stop_cursor = cursor_x;
    }
 
    matrix.fillScreen(LOW);
    matrix.setCursor(cursor_x, matrix_font_shift);
    //Serial.println("before print");
    matrix.print(matrix_message);
    //Serial.println("after print");
    matrix.write();
    //Serial.println("after write");
    matrix_scroll++;
 }
#endif
}

void ulisp_message_set(const char* message, unsigned long duration) {
#ifdef  USEMAX72
  matrix_message_len = strlen(message);
  matrix_message_starttime = millis();
  matrix_message_stoptime = matrix_message_starttime + duration;
  if(matrix_message != NULL) {
    free(matrix_message);
    matrix_stop_cursor = 0;
  }
  matrix.shutdown(false);
  //Serial.println("before malloc");
  matrix_message = (char*) malloc(matrix_message_len + 1);
  //Serial.println("after malloc");
  memcpy(matrix_message, message, matrix_message_len + 1);

  int16_t  x1, y1;
  uint16_t w, h;

  //Serial.println("before getBounds");
  matrix.getTextBounds(matrix_message, 0, 0, &x1, &y1, &w, &h);
  //Serial.println("after getBounds");
  matrix_message_width = w;
  matrix_message_height = h;
#endif
}

double ulisp_temperature_read() {
#ifdef USEHDC1080
  return hdc1080.readTemperature();
#else
  return -277;
#endif
}

double ulisp_humidity_read() {
#ifdef USEHDC1080
  return hdc1080.readHumidity();
#else
  return -1;
#endif
}

uint32_t ulisp_light_read() {
#ifdef USEBH1750
  lux.setOnceHigh2Res();
  return lux.getLux();
#elif defined USETEMP6000
  return analogRead(ADC_PIN);
#else
  return -1;
#endif
}

#ifdef USECCS811
unsigned long last_ccs_update = millis();
int32_t last_co2 = 0;
int32_t last_tvoc = 0;

void ccs_update() {
  unsigned long now = millis();
  if((now - last_ccs_update) > 500) {
    if(ccs.available()){
      ccs.setEnvironmentalData((uint8_t) ulisp_humidity_read(), ulisp_temperature_read());
      if(!ccs.readData()){
        last_co2 = ccs.geteCO2();
        last_tvoc = ccs.getTVOC();
        //Serial.print("sleep duration => ");
        //Serial.println(now - last_ccs_update);
        last_ccs_update = now;
      } else{
        Serial.print(" !!!ccs update ERROR!!! ");
      }
    }  
  }
}
#endif

int32_t ulisp_co2_read() {
#ifdef USECCS811
  ccs_update();
  return last_co2;
#else
  return -1;
#endif
}

int32_t ulisp_tvoc_read() {
#ifdef USECCS811
  ccs_update();
  return last_tvoc;
#else
  return -1;
#endif
}

char* ulisp_funcs[] = {
  "message",
#ifdef USEMAX72
  "show",
#endif
#ifdef USEHDC1080
  "temperature-read",
  "humidity-read",
#endif
#ifdef USECCS811
  "tvoc-read",
  "co2-read",
#endif
#ifdef USEBH1750
  "light-read",
#endif
#ifdef USETEMP6000
  "light-read",
#endif
#ifdef USEVL53L0X
#endif
#ifdef USEIRR
#endif
#ifdef USEIRS
#endif
  NULL
};

char** ulisp_discovery() {
  return ulisp_funcs;
}

// setup and loop

void setup() {
  Serial.begin(115200);
  Serial.println("\nbootloader complete => setup started");

  rst_info *resetInfo = ESP.getResetInfoPtr();
  Serial.print("Reset reason: ");
  Serial.print(resetInfo->reason);
  Serial.print(", exccause: ");
  Serial.println(resetInfo->exccause);

  fs_setup();
  uint8_t have_config = fs_parseconfig();
  use_crypto = have_config;

#ifdef USEMAX72
  matrix.setIntensity(15);
  matrix.setPosition(0, 0, 0);
  matrix.setRotation(0, 1);
  matrix.setPosition(1, 1, 0);
  matrix.setRotation(1, 1);
  matrix.setPosition(2, 2, 0);
  matrix.setRotation(2, 1);
  matrix.setPosition(3, 3, 0);
  matrix.setRotation(3, 1);
  matrix.setPosition(4, 4, 0);
  matrix.setRotation(4, 1);
  matrix.fillScreen(HIGH);
  //matrix_font_shift = 0;
  matrix.setFont(&Picopixel); matrix_font_shift = 6;
  //matrix.setFont(&TomThumb); matrix_font_shift = 7;
  matrix.cp437(true);
  matrix.setTextColor(HIGH, LOW);
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.write();
#endif

#ifdef USEIRS
  irsend.begin();
  irsend.sendSAMSUNG(0xE0E048B7, 32);
  delay(40);
  irsend.sendSAMSUNG(0xE0E048B7, 32);
  delay(40);
  irsend.sendSAMSUNG(0xE0E048B7, 32);
  delay(40);
#endif

#ifdef USEIRR
  irrecv.enableIRIn();
  irrecv.setUnknownThreshold(12);
  Serial.println("irrecv.enableIRIn");
#endif

  Wire.begin();

#ifdef USEBUTTON
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_change, CHANGE);
#endif

#ifdef USELED
  pinMode(LED_PIN, OUTPUT);
  if(! use_crypto) {
    digitalWrite(LED_PIN, 1);
  }
#endif

  /*
  pinMode(14, OUTPUT);
  digitalWrite(14, 1);
  pinMode(12, OUTPUT);
  digitalWrite(12, 1);
  */

#ifdef USETEMP6000
  pinMode(ADC_PIN, INPUT);
#endif

#ifdef USEHDC1080
  hdc1080.begin(0x40);
#endif

#ifdef USEVL53L0X
  distance.setTimeout(500);
  if (!distance.init()){
    Serial.println("Failed to detect and initialize distance!");
  }
#endif

#ifdef  USECCS811
  if(!ccs.begin()){
    Serial.println("Failed to start sensor! Please check your wiring.");
  }
#endif

#ifdef USEBH1750
  lux.powerOn();
#endif

  ulisp_setup();

  // Wi-Fi
  if(have_config) {
    wifi_sta(cfg["ssid"], cfg["password"]);
    aes_setup(cfg["key"]);
    String msg = String("WiFi STA IP: ");
    msg.concat(WiFi.localIP().toString());
    ulisp_message_set(msg.c_str(), 2000);
  } else {
    wifi_ap();
  }

  periodic_setup();
  udp_setup();
  mdns_setup();
}

/*
void dump_loop() {
  Serial.print("T=");
  Serial.print(temperature_read());
  Serial.print("C, RH=");
  Serial.print(humidity_read());

  Serial.print(" CO2: ");
  Serial.print(co2_read());
  Serial.print("ppm, TVOC: ");
  Serial.print(tvoc_read());
  Serial.print(" L=");
  Serial.print(light_read());
  Serial.print("lx dist=");

  uint32_t dst = distance.readRangeSingleMillimeters();
  if(dst > 8000) {
    digitalWrite(LED_PIN, 0);
    Serial.print("nothing");
  } else {
    if(dst <= 330) {
      analogWrite(LED_PIN, 1000 - dst * 3 );
    } else {
      analogWrite(LED_PIN, 0);
    }
    Serial.print(dst);
    Serial.print("mm");
  }
  Serial.println("");
}
*/


void loop() {
  //Serial.println("on loop");

#ifdef  USEIRR
  if (irrecv.decode(&results)) {
    Serial.print(resultToHumanReadableBasic(&results));
    Serial.println(resultToSourceCode(&results));
    Serial.println(getCorrectedRawLength(&results));
    yield();
  }
#endif

#ifdef  USEIRS
  irsend.sendSAMSUNG(0xE0E040BF, 32);
#endif

  udp_loop();
  mdns_loop();

  periodic_loop();

  message_loop();
  button_loop();
  //dump_loop();
  delay(10);
}
