#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char** argv)
{
    int n;
    FILE *fout = NULL;

    if(argc != 2) {
      printf("Usage: ./generate <# elements>\n");
      return -1;
    }

    n = atoi(argv[1]);

    printf("n = %d\n", n);

    srand(time(NULL));
    fout = fopen("input.txt", "w");
    fprintf(fout, "%d\n", n);
    while(n--)
    {
      fprintf(fout, "%d\n", rand() % 1000000);
    }
}
