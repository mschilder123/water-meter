water-meter: water-meter.c
        clang-format -i $<
        gcc -o $@ $< -Wall -Werror -lwiringPi -pthread
        sudo setcap 'cap_sys_nice=pe' $@

clean:
        rm water-meter
