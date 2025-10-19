// Event counter monitoring a I/O pin.
//
// Idle pin is pulled up.
// Event registered when pin is seen pulled down.
//
// Some debounce oversampling to cut noise.

#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>
#include <wiringPi.h>

#define PULSE_PIN 4 // Using GPIO Pin 23, which is Pin 4 for wiringPi library
#define SAMPLE_RATE 1000 // in micros, i.e 1000 = 1ms

#define WWW_FILE "/var/www/html/well.json"
#define WWW_DELTA_FILE "/var/www/html/welldeltadata.json"

// Try and get real-time high priority.
// If fails, suggest a setcap commandline.
void try_boost_priority() {
  struct sched_param sched_params = {0};
  int sched_policy = SCHED_RR;
  int err;

  sched_params.sched_priority = sched_get_priority_max(sched_policy);

  err = pthread_setschedparam(pthread_self(), sched_policy, &sched_params);
  if (err != 0)
    perror("pthread_setschedparam()");

  err = pthread_getschedparam(pthread_self(), &sched_policy, &sched_params);
  if (err != 0)
    perror("pthread_getschedparam()");

  if (err == 0 && sched_policy != SCHED_RR) {
    fprintf(stderr, "$ sudo setcap 'cap_sys_nice=pe' $this-executable-file\n"
                    "  to get better sample timing accuracy.\n");
  }
}

// Thread that samples our I/O pin at fixed interval.
// Runs forever loop at real-time high priority.
//
// Can discern events as quick as 32+3 ticks (e.g. 28Hz at 1ms sampling rate),
// but reports them with second granularity timestamps.
void *sampler_pthread(void *thread_arg) {
  int fd = (int)thread_arg;

  uint32_t currentSamples = 0;
  int currentState = 0;

  try_boost_priority();

  while (1) {
    int currentLevel = digitalRead(PULSE_PIN);

    // Track most recent 32 samples.
    currentSamples = (currentSamples << 1) | currentLevel;

    if (currentState == 0 && currentSamples == -1u) {
      // 32 idle samples seen; we're idle again.
      currentState = 1;
    }

    if (currentState == 1 && (currentSamples & 7) == 0) {
      // Some consequetive zeros seen; we're busy.
      currentState = 0;

      // Send event timestamp to pipe with main thread.
      time_t now = time(NULL);
      write(fd, &now, sizeof(now));
    }

    usleep(SAMPLE_RATE);
  }
}

// Main loop that reads event timestamps from pipe.
//
// Tracks total events and writes to files and stdout.
// Runs forever at normal priority.
int main_thread(int argc, char *argv[], int sampler_fd) {
  uint32_t totalEvents = 0;
  if (argc > 1)
    totalEvents = atoi(argv[1]);

  uint32_t prevEventMillis = INT_MIN; // to make initial rate compute to 0
  time_t prevEventTime = 0;

  while (1) {
    time_t when = time(NULL);

    if (prevEventTime == 0 /* initial loop */ ||
        read(sampler_fd, &when, sizeof(when)) == sizeof(when)) {

      if (prevEventTime != 0)
        ++totalEvents;

      // Compute events per minute using millis(),
      // as seen arriving at this receiver.
      uint32_t currentMillis = millis();
      uint32_t deltaMillis = currentMillis - prevEventMillis;
      float eventsPerMinute = 60000.0f / deltaMillis;

      // Construct json for this event.
      // Write to file and stdout.
      {
        char buf[1024];

        sprintf(buf, "{\"time\": %lu, \"ctime\": \"", when);
        ctime_r(&when, buf + strlen(buf));
        sprintf(buf + strlen(buf) - 1, "\", \"total\": %u, \"rpm\": %.1f}\n",
                totalEvents, eventsPerMinute);

        printf("%s", buf);
        fflush(stdout);

        int fd = open(WWW_FILE, O_WRONLY | O_TRUNC);
        if (fd >= 0) {
          write(fd, buf, strlen(buf));
          close(fd);
        }
      }

      // Update delta json file.
      {
        char buf[1024];

        static const char *POSTFIX = "]\n";
        if (prevEventTime == 0) {
          // program (re)start: write starting status with negative numbers.
          snprintf(buf, sizeof(buf), ",\n-%lu,-%u%s", when, totalEvents,
                   POSTFIX);
        } else {
          // normal event: write only delta seconds.
          snprintf(buf, sizeof(buf), ",%lu%s", when - prevEventTime, POSTFIX);
        }

        int fd = open(WWW_DELTA_FILE, O_RDWR);
        if (fd >= 0) {
          lseek(fd, -strlen(POSTFIX), SEEK_END);
          write(fd, buf, strlen(buf));
          close(fd);
        }
      }

      prevEventMillis = currentMillis;
      prevEventTime = when;
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {
  wiringPiSetup();

  // configure pin
  pullUpDnControl(PULSE_PIN, PUD_UP);
  pinMode(PULSE_PIN, INPUT);

  // get pipe
  int pip[2];
  pipe(pip);

  // run threads
  pthread_t sampler;
  pthread_create(&sampler, NULL, sampler_pthread, (void *)pip[1]);
  return main_thread(argc, argv, pip[0]);
}
