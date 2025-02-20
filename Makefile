.PHONY: clean

out: calc case_all
	./calc < case_all

case_add: casegen
	./casegen add 100 > case_add

case_sub: casegen
	./casegen sub 100 > case_sub

case_mul: casegen
	./casengen mul 100 > case_mul

case_div: casegen
	./casengen div 100 > case_div

case_all: 
	./casegen add 100 > case_all
	./casegen sub 100 >> case_all
	./casegen mul 100 >> case_all
	./casegen div 100 >> case_all

casegen: casegen.c
	gcc casengen.c -o casegen

calc: calc.c
	gcc calc.c -o calc

clean:
	rm -f out calc casegen case_* *.o
