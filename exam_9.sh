if (($# == 1))
	cat stderr.txt
fi

s=${1}[1]

if (($# == 2))
	sed "${s},\$p" stderr.txt
fi

t=${2}[1]

if (($# == 3))
	sed "${t},${2}p" stderr.txt
