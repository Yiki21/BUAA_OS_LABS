all: fibo.o main.o
include = ../include

%.o: %.c
	$(CC) -I$(include) -c $< -o $@
