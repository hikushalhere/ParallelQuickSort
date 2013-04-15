make; make clean all
./generate 10000000
for i in {1..64..1}
do
	echo $i
	./run $i
done
#./run 4
