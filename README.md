# water-meter
raspberry pi water meter

Supports simple pull-up falling edge pulse counting.
No extra hardware needed; using internal pull-up at 3.3v.
Assumes the pulses made by the meter are a simple switch that either is open or tied to GND.

I'm using this with a https://mcsmeters.com/collections/water-meters-1-2-to-2/products/mcs-1-poly-water-sub-meter-w-pulse-us-gallons to good effect.
That one appears to have about 25% pull-low duty cycle and lots of bounce on 1 -> 0 edges, less so for 0 -> 1 edges.

BUILD:
$ sudo apt install wiringpi clang-format
$ make

RUN:
$ ./water-meter start-total-value [--debug]
