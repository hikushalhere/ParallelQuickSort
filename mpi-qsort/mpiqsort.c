#include "header.h"

typedef struct
{
	int *arr1;
	int *arr2;
	int *result;
	int arr1Len;
	int arr2Len;
} mergeArgs;

/* Comparer */
int compare(const void *a, const void *b)
{
    return (*(int*)a - *(int*)b);
}

// Binary search to find the location of the parition point of the array
int binSearch(int *arr, int value, int left, int right)
{
    int mid = left + (right - left) / 2;
    while(left <= right && value != arr[mid])
    {
        if(value < arr[mid])
            right = mid - 1;
        else
            left = mid + 1;

        mid = left + (right - left) / 2;
    }
    return mid;
}

/* Find if I am in small or large category */
int inSmallCategory(int commRank, int root, int commSize)
{
	return (commRank < (root + commSize / 2));
}

void *mergeArrays(void *arg)
{
	mergeArgs *args = (mergeArgs *) arg;
	int pos = 0, pos1 = 0, pos2 = 0;
	while(pos1 < args->arr1Len && pos2 < args->arr2Len)
	{
		if(args->arr1[pos1] < args->arr2[pos2])
			args->result[pos++] = args->arr1[pos1++];
		else
			args->result[pos++] = args->arr2[pos2++];
	}
	while(pos1 < args->arr1Len)
		args->result[pos++] = args->arr1[pos1++];
	while(pos2 < args->arr2Len)
		args->result[pos++] = args->arr2[pos2++];
	pthread_exit(0);
}

/* Merge two sorted arrays into one */
int *mergeArray(int *arr1, int arr1Len, int *arr2, int arr2Len, int len)
{
	int *result = (int *) malloc(len * sizeof(int));
	mergeArgs *args1 = (mergeArgs *) malloc(sizeof(mergeArgs) * 2);
	mergeArgs *args2 = (mergeArgs *) malloc(sizeof(mergeArgs));
	int pivot1 = arr1Len / 2;
	int pivot2 = binSearch(arr2, arr1[arr1Len / 2], 0, arr2Len - 1);
	
	args1->arr1 = arr1;
	args1->arr2 = arr2;
	args1->result = result;
	args1->arr1Len = pivot1;
	args1->arr2Len = pivot2;
	
	args2->arr1 = arr1 + pivot1;
	args2->arr2 = arr2 + pivot2;
	args2->result = result + pivot1 + pivot2;
	args2->arr1Len = arr1Len - pivot1;
	args2->arr2Len = arr2Len - pivot2;
	
	pthread_t *prefixThreads = (pthread_t *) malloc(sizeof(pthread_t) * 2);
	pthread_create(&prefixThreads[0], NULL, mergeArrays, (void *) args1);
	pthread_create(&prefixThreads[1], NULL, mergeArrays, (void *) args2);
	pthread_join(prefixThreads[0], NULL);
    pthread_join(prefixThreads[1], NULL);
    
    free(args1);
    free(args2);
	return result;
}

int* mpiqsortHelper(int* input, int* dataLengthPtr, MPI_Comm comm, int commRank, int commSize, int root)
{
	if(commSize == 1)
		return input;
	
	int pivot, sendCount, recvCount, firstCount, secondCount, dataLength = *dataLengthPtr;
	int *first, *second, *recvBuf = NULL;
	MPI_Status status;
	
	/* Broadcast pivot if rank is the root*/
	if(commRank == root)
	{
		pivot = input[dataLength / 2];
		int dest;
		for(dest = root + 1; dest < root + commSize; dest++)
			MPI_Send(&pivot, 1, MPI_INT, dest, 0, comm);
	}
	/* Receive pivot if rank is anything but root */
	else
		MPI_Recv(&pivot, 1, MPI_INT, root, 0, comm, &status);
		
	/* Locate the pivot in the array */
	int partitionPoint = binSearch(input, pivot, 0, dataLength - 1);
	
	/* Find if the current process falls in the category for handling elements smaller than the pivot or not */
	int inSmall = inSmallCategory(commRank, root, commSize);
	
	/* If the processor is going to handle the elemetns smaller than the pivot */
	if(inSmall)
	{
		/* First send... */
		int peer  = commRank + commSize / 2;                                 // Compute the rank of the process with which to exchange data
		sendCount = dataLength - partitionPoint;                             // Compute the number of elements to send
		MPI_Send(&sendCount, 1, MPI_INT, peer, 1, comm);                     // Send the number of elements that the other processor should expect
		MPI_Send(input + partitionPoint, sendCount, MPI_INT, peer, 1, comm); // Send the elements greater than the pivot
		
		/* ...  and then receive */
		MPI_Recv(&recvCount, 1, MPI_INT, peer, 1, comm, &status);            // Fetch the number of elements to be received
		recvBuf = (int *) malloc(recvCount * sizeof(int));                   // Allocate memory to receive data
		MPI_Recv(recvBuf, recvCount, MPI_INT, peer, 1, comm, &status);       // Accept the elements smaller than the pivot
		
		/* Set the first and second half of the resultant array and their elements counts */
		first  = input;
		second = recvBuf;
		firstCount  = partitionPoint;
		secondCount = recvCount;
	}
	else
	{
		/* First receive... */
		int peer = commRank - commSize / 2;                            // Compute the rank of the process with which to exchange data
		MPI_Recv(&recvCount, 1, MPI_INT, peer, 1, comm, &status);      // Fetch the number of elements to be received
		recvBuf = (int *) malloc(recvCount * sizeof(int));             // Allocate memory to receive data
		MPI_Recv(recvBuf, recvCount, MPI_INT, peer, 1, comm, &status); // Accept the elements greater than the pivot
		
		/* ...  and then send */
		sendCount = partitionPoint;                                    // Compute the number of elements to send
		MPI_Send(&sendCount, 1, MPI_INT, peer, 1, comm);               // Send the number of elements that the other processor should expect
		MPI_Send(input, sendCount, MPI_INT, peer, 1, comm);            // Send the elements smaller than the pivot
		
		/* Set the first and second half of the resultant array and their elements counts */
		first  = recvBuf;
		second = input + partitionPoint;
		firstCount  = recvCount;
		secondCount = dataLength - partitionPoint;
	}
	
	/* Merge the elements in the array */
	dataLength = firstCount + secondCount;
	int *newInput = mergeArray(first, firstCount, second, secondCount, dataLength);
	
	/* Memory cleanup */
	free(input);
	if(recvBuf)
		free(recvBuf);
		
	/* Update the variables and recurse */
	*dataLengthPtr = dataLength;
	if(!inSmall)
		root += commSize / 2;
	return mpiqsortHelper(newInput, dataLengthPtr, comm, commRank, commSize / 2, root);
}

int* mpiqsort(int* input, int globalNumElements, int* dataLengthPtr, MPI_Comm comm, int commRank, int commSize)
{
  qsort(input, *dataLengthPtr, sizeof(int), compare);
  int *output = mpiqsortHelper(input, dataLengthPtr, comm, commRank, commSize, 0);
  //printf("\nfinal data len is %d", *dataLengthPtr);
  return output;
}

