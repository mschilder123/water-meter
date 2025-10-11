#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wiringPi.h>

#define PULSE_PIN 4 // Using GPIO Pin 23, which is Pin 4 for wiringPi library
#define SAMPLE_RATE 1000 // micros

#define WWW_FILE "/var/www/html/well.json"

bool FLAG_debug; // --debug

volatile int totalGallons = 0;
volatile unsigned int lastUpdateMillis = 0;
volatile float gallonsPerMinute = 0;
volatile int currentState = 0;
volatile unsigned int currentSamples = 0;

struct event_t {
  int millis;
  int level;
};

#define NUM_EVENTS 128
struct event_t events[NUM_EVENTS];
volatile int event_wp, event_rp;

void push_event(int millis, int level) {
  if ((event_wp + 1) % NUM_EVENTS != event_rp) {
    struct event_t e = {.millis = millis, .level = level};
    events[event_wp] = e;
    event_wp = (event_wp + 1) % NUM_EVENTS;
  } else {
    // event fifo full
  }
}

bool pop_event(struct event_t *d) {
  if (event_rp == event_wp) {
    // event fifo empty
    return false;
  }
  *d = events[event_rp];
  event_rp = (event_rp + 1) % NUM_EVENTS;
  return true;
}

void sigAlarm(int sig_num) {
  int currentLevel = digitalRead(PULSE_PIN);

  if (sig_num != SIGALRM)
    return;

  currentSamples = (currentSamples << 1) | currentLevel;

  if (currentState == 0 && currentSamples == -1u) {
    // 32 idle samples seen; we're idle again
    currentState = 1;
  }

  if (currentState == 1 && (currentSamples & 7) == 0) {
    // Some consequetive zeros seen; we're busy
    unsigned int now = millis();

    currentState = 0;

    gallonsPerMinute = 60000.0f / (now - lastUpdateMillis);
    lastUpdateMillis = now;
    totalGallons++;
  }
}

bool debugEvents(void) {
  bool had_output = false;

  if (FLAG_debug) {
    struct event_t e;
    while (pop_event(&e)) {
      printf("[%u,%d]", e.millis, e.level);
      had_output = true;
    }
  }

  return had_output;
}

void printState(void) {
  static int last_count = -1;
  bool add_eoln = false;

  add_eoln = debugEvents();

  if (last_count != totalGallons) {
    last_count = totalGallons;

    time_t now = time(NULL);
    char *t = ctime(&now);
    *strchr(t, '\n') = 0;

    char buf[1024];
    snprintf(buf, sizeof(buf),
             "{\"time\": %lu, \"ctime\": \"%s\", \"millis\": %u, \"total\": "
             "%u, \"gpm\": %.1f}\n",
             now, t, lastUpdateMillis, last_count, gallonsPerMinute);
    printf("%s", buf);
    add_eoln = false;

    // update www file
    int fd = open(WWW_FILE, O_WRONLY | O_TRUNC);
    if (fd >= 0) {
      write(fd, buf, strlen(buf));
      close(fd);
    }
  }

  if (add_eoln)
    printf("\n");
  fflush(stdout);
}

int main(int argc, char *argv[]) {
  // optional --debug
  if (argc > 1) {
    if (!strcmp(argv[argc - 1], "--debug")) {
      FLAG_debug = true;
      --argc;
    }
  }
  // optionally set start total
  if (argc > 1) {
    totalGallons = atoi(argv[1]);
  }

  if (wiringPiSetup() < 0) {
    fprintf(stderr, "Unable to setup wiringPi: %s\n", strerror(errno));
    return 1;
  }

  // configure pin
  pullUpDnControl(PULSE_PIN, PUD_UP);
  pinMode(PULSE_PIN, INPUT);

  lastUpdateMillis = millis() - 1;

  // set up sampler
  signal(SIGALRM, sigAlarm);
  ualarm(SAMPLE_RATE, SAMPLE_RATE);

  // loop forever, mostly sleeping
  while (1) {
    printState();
    delay(1000); // wait 1 second
  }

  return 0;
}
