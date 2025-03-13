i=0
while ((i <= 20))
do
	touch result/code/${i}.c
	sed "s/REPLACE/${i}/g" origin/code/${i}.c > result/code/${i}.c
	rm origin/code/${i}.c
	((i++))
done

