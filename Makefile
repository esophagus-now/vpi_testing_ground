all: hello.vvp my_task.vpi

timeout_rw.o: timeout_rw.h timeout_rw.c
	gcc -Wall -c -fpic -fno-diagnostics-show-caret timeout_rw.c 

my_task.o: my_task.c timeout_rw.h timeout_rw.c
	gcc -Wall -c -fpic -fno-diagnostics-show-caret my_task.c -I/usr/include/iverilog -IC:\MinGW\msys\1.0\local\include\iverilog 
	
my_task.vpi: my_task.o timeout_rw.o
	gcc -shared -o my_task.vpi my_task.o timeout_rw.o -LC:\MinGW\msys\1.0\local\lib -lvpi -lrt

hello.vvp:	hello.v
	iverilog -o hello.vvp hello.v

test.vcd: hello.vvp my_task.vpi
	vvp -M. -mmy_task hello.vvp
	
run: hello.vvp my_task.vpi
	vvp -M. -mmy_task hello.vvp
	
clean:
	rm -rf *.o
	rm -rf *.vpi
	rm -rf *.vvp
	rm -rf *.vcd

open:	test.vcd
	gtkwave test.vcd --autosavename &
