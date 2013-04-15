all: generate run

generate: generate.c
	gcc -o generate generate.c

pqsort.o: pqsort.c
	gcc -c -lm -lpthread pqsort.c

run: driver.c pqsort.o
	gcc -o run driver.c pqsort.o -lm -lpthread

clean:
	rm generate pqsort.o run
