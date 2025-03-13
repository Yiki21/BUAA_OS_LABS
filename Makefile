all:	
	gcc -c check.c -o check.o
	gcc src/output.c src/main.c -Isrc/include/ -o src/main
	cp src/main out/main
	rm src/main	

check: check.c
	gcc -c check.c -o check.o
run: out/main
	./out/main

clean: check.o out/main
	rm check.o
	rm out/main
