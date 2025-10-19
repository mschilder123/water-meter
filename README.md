# water-meter
raspberry pi water meter

Supports simple pull-up being pulled down monitoring.
No extra hardware needed; using internal pull-up at 3.3v.
Assumes the pulses are from a simple drag contact that either is open or tied to GND.

Using this with a https://mcsmeters.com/collections/water-meters-1-2-to-2/products/mcs-1-poly-water-sub-meter-w-pulse-us-gallons to good effect.
That one appears to have about 25% pull-low duty cycle and lots of bounce noise.
Debounce is done with simple oversampling: need to see 3 consequetive samples low before we signal and then 32 consequetive samples high again to go back to idle state.

Events are bookkept in a json file in a simple delta compressed format, ready for ingestion by a charting webpage.

BUILD:
$ sudo apt install wiringpi clang-format
$ make

RUN:
$ ./water-meter start-total-value
