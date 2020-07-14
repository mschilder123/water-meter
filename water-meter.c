#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wiringPi.h>

// Using GPIO Pin 23, which is Pin 4 for wiringPi library

#define PULSE_PIN 4
#define DEBOUNCE_MILLIS 1000 // 50 G/M max pulse rate

#define WWW_FILE "/var/www/html/well.json"

volatile int totalGallons = 0;
volatile unsigned int lastUpdateMillis = 0;
volatile unsigned int lastBounceMillis = 0;
volatile float gallonsPerMinute = 0;

void FallingEdgeInterrupt(void) {
  unsigned int now = millis();
  unsigned int delta_t = now - lastBounceMillis;
  if (delta_t > DEBOUNCE_MILLIS) {
    gallonsPerMinute = 60000.0f / (now - lastUpdateMillis);
    lastUpdateMillis = lastBounceMillis = now;
    totalGallons++;
  }
}

void RisingEdgeInterrupt(void) {
  unsigned int now = millis();
  unsigned int delta_t = now - lastBounceMillis;
  if (delta_t > DEBOUNCE_MILLIS) {
    lastBounceMillis = now;
  }
}

void printState(void) {
  static int last_count = -1;

  if (last_count != totalGallons) {
    last_count = totalGallons;

    time_t now = time(NULL);
    char *t = ctime(&now);
    *strchr(t, '\n') = 0;

    char buf[1024];
    snprintf(
        buf, sizeof(buf),
        "{\"time\": %lu, \"ctime\": \"%s\", \"total\": %u, \"gpm\": %.1f}\n",
        now, t, last_count, gallonsPerMinute);
    printf("%s", buf);

    // update www file
    int fd = open(WWW_FILE, O_WRONLY);
    if (fd >= 0) {
      write(fd, buf, strlen(buf));
      close(fd);
    }
  }
}

int main(int argc, char *argv[]) {
  // optionally set total
  if (argc > 1) {
    totalGallons = atoi(argv[1]);
  }

  if (wiringPiSetup() < 0) {
    fprintf(stderr, "Unable to setup wiringPi: %s\n", strerror(errno));
    return 1;
  }

  // configure PIN
  pinMode(PULSE_PIN, INPUT);
  pullUpDnControl(PULSE_PIN, PUD_UP);

  // setup notification
  if (wiringPiISR(PULSE_PIN, INT_EDGE_FALLING, &FallingEdgeInterrupt) < 0) {
    fprintf(stderr, "Unable to setup ISR: %s\n", strerror(errno));
    return 1;
  }
  if (wiringPiISR(PULSE_PIN, INT_EDGE_RISING, &RisingEdgeInterrupt) < 0) {
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