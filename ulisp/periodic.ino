
#define PERIODIC_COUNT 5

struct periodic {
  uint64_t period;
  uint64_t exec_count;
  char*    code;
  uint64_t start;
  bool     active;
};

struct periodic tasks[PERIODIC_COUNT];

void periodic_setup() {
  for(uint16_t i = 0; i < PERIODIC_COUNT; i++) {
    tasks[i].active = false;
  }
}

void periodic_loop() {
  unsigned long now = millis();
  for(uint16_t i = 0; i < PERIODIC_COUNT; i++) {
    if(tasks[i].active == true 
      && tasks[i].exec_count != ((tasks[i].start - now) / tasks[i].period)) {
      tasks[i].exec_count = ((tasks[i].start - now) / tasks[i].period);
      Serial.printf("running code %s\n", tasks[i].code);
      ulisp_run(tasks[i].code);
    }
  }
}

uint16_t ulisp_periodic_task(uint64_t period, const char* code) {
  ssize_t len = strlen(code) + 1;
  bool    ok = false;
  for(uint16_t i = 0; i < PERIODIC_COUNT; i++) {
    if(tasks[i].active == false) {
      tasks[i].start = millis();
      tasks[i].active = true;
      tasks[i].period = period;
      tasks[i].exec_count = 1;

      ssize_t len = strlen(code) + 1;
      tasks[i].code = (char*) malloc(len);
      memcpy(tasks[i].code, code, len);

      ok = true;
      break;
    }
  }
  if(!ok) {
    return 1;
  }
  return 0;
}
