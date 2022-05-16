PHONY: emulator

emulator: emulator.c emulator.h main.c
	gcc -g emulator.c main.c -o emulator

clean:
	rm -r *.o emulator *.dSYM
