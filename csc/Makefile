.PHONY: clean
fibo: 
	$(MAKE) -C code
	$(CC) ./code/fibo.o ./code/main.o -o fibo

clean:
	rm ./code/*.o
