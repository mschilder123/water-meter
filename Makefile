water-meter: water-meter.c
	clang-format -i $<
	gcc -o $@ $< -Wall -Werror -lwiringPi
