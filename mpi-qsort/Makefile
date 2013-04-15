all: generate run validate

generate: generate.c
	gcc -o generate generate.c

run: driver.c mpiqsort.c header.h
	mpicc -o run -lm -lpthread driver.c mpiqsort.c

validate: validate.c
	gcc -o validate validate.c

clean:
	rm generate run validate
