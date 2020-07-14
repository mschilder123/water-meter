# water-meter
raspberry pi water meter

Supports simple pull-up falling edge pulse counting.
No extra hardware needed; using internal pull-up at 3.3v.
Assumes the pulses made by the meter are a simple switch that either is open or tied to GND.
Max pulse rate ~50 bpm. Debounces on both rising and falling edges.

I'm using this with a https://mcsmeters.com/collections/water-meters-1-2-to-2/products/mcs-1-poly-water-sub-meter-w-pulse-us-gallons to good effect.

Needs:
$ sudo apt install wiringpi clang-format
