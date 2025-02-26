all: fibo.o main.o
include = ../include

%.o: %.c
	gcc -I$(include) -c $< -o $@
