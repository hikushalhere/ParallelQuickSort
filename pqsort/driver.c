#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "header.h"

void validate(int* output, int num_elements) {
  int i = 0;
  assert(output != NULL);
  for(i = 0; i < num_elements - 1; i++) {
    if (output[i] > output[i+1]) {
      printf("************* NOT sorted *************\n");
      printf("%d %d %d",i, output[i], output[i+1]);
      return;
    }
  }
  printf("============= SORTED ===========\n"); 
}

int main(int argc, char **argv)
{
    FILE* fin = NULL;
    int* input = NULL;
    int* output = NULL;
    int num_elements, num_threads, i = 0;

    if(argc != 2)
      {printf("Usage: ./pqsort <num of threads>\n");}
    
    num_threads = atoi(argv[1]);

    //read input_size and input
    if((fin = fopen("input.txt", "r")) == NULL)
      {printf("Error opening input file\n"); exit(0);}

    fscanf(fin, "%d", &num_elements);
    if( !(input = (int *)calloc(num_elements, sizeof(int))) )
      {printf("Memory error\n"); exit(0);}

    for(i = 0; i < num_elements || feof(fin); i++)
        fscanf(fin, "%d", &input[i]);
    
    if(i < num_elements)
      {printf("Invalid input\n"); exit(0);}

    struct timeval start, end;

    gettimeofday(&start, NULL);
    output = pqsort(input, num_elements, num_threads);
    gettimeofday(&end, NULL);
    long int time = (end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec);
    printf("\nTime taken (in micro-secs): %ld\n", time);
    printf("\nSpeedup: %f\n", (float) 4028247 / time);    
    validate(output, num_elements);
    return 0;
}
