#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wiringPi.h>

#define PULSE_PIN 4 // Using GPIO Pin 23, which is Pin 4 for wiringPi library
#define DEBOUNCE_MILLIS 50

#define WWW_FILE "/var/www/html/well.json"

bool FLAG_debug; // --debug

volatile int totalGallons = 0;
volatile unsigned int lastUpdateMillis = 0;
volatile unsigned int lastBounceMillis = 0;
volatile float gallonsPerMinute = 0;
volatile int currentState = 1;

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

void EdgeInterrupt(void) {
  unsigned int now = millis();
  unsigned int delta_t = now - lastBounceMillis;
  int currentLevel = digitalRead(PULSE_PIN);

  if (FLAG_debug) {
    push_event(now, currentLevel);
  }

  if (delta_t > DEBOUNCE_MILLIS) {
    if (currentLevel == 0) {
      if (currentState == 1) {
        gallonsPerMinute = 60000.0f / (now - lastUpdateMillis);
        lastUpdateMillis = now;
        totalGallons++;
        currentState = 0;
        lastBounceMillis = now;
      }
    } else if (currentState == 0) {
      currentState = 1;
      lastBounceMillis = now;
    }
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
  pinMode(PULSE_PIN, INPUT);
  pullUpDnControl(PULSE_PIN, PUD_UP);

  // sample pin
  currentState = digitalRead(PULSE_PIN);

  // setup notification
  if (wiringPiISR(PULSE_PIN, INT_EDGE_BOTH, &EdgeInterrupt) < 0) {
    fprintf(stderr, "Unable to setup ISR: %s\n", strerror(errno));
    return 1;
  }

  // loop forever, mostly sleeping
  while (1) {
    printState();
    delay(1000); // wait 1 second
  }

  return 0;
}
