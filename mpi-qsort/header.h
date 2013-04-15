#ifndef __HEADER_H__
#define __HEADER_H__

#include <assert.h>
#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_PROC 16

extern int* mpiqsort(int*, int, int*, MPI_Comm, int, int);

#endif
