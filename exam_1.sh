#!bin/bash

mkdir result
cd result
mkdir code
mkdir backup
cd ..

sed -n "s/hello p" origin/basic.c

mv origin/basic.c result/

i=0
while (( i <= 20))
do
	sed -n "s/REPLACE/${i}/g" origin/code/${i}.c
	mv origin/code/${i}.c result/code/${i}.c
	((i++))
done


gcc -o result/verify result/code/0.c result/code/1.c result/code/2.c result/code/3.c \
result/code/4 result/code/5.c result/code/6.c result/code/7.c result/code/8.c result/code/9.c \
result/code/10.c result/code/11.c result/code/12.c result/code/13.c result/code/14.c \
result/code/15.c result/code/16.c result/code/17.c result/code/18.c result/code/19.c result/code/20.c 

./result/verify 2> stderr.txt

chmod stderr.txt 440


