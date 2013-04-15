#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#define MEDIAN_SET_SIZE 50
#define MAX_QSORT_THREADS 256 

int globalNbyP = 0;

pthread_mutex_t spareThreadsLock;
int spareThreads = 0;

pthread_mutex_t qSortThreadsLock;
pthread_t qSortThreads[MAX_QSORT_THREADS];
int qSortThreadsNum = -1;

pthread_mutex_t partitionThreadsLock;
pthread_t partitionThreads[MAX_QSORT_THREADS];
int partitionThreadsNum = -1;

typedef struct
{
    int *a;
    int *aCopy;
    int left;
    int right;
    int num_threads;
    int thread;
}pqsortHelperArgs;

typedef struct
{
    int *a;
    int *aCopy;
    int aLen;
    int globalLeft;
    int left;
    int right;
    int *isSmallMap;
    int smallPrefixSum;
    int largePrefixSum;
    int totLarge;
} moveElementsArgs;

typedef struct
{
    int *a;
    int globalLeft;
    int left;
    int right;
    int pivot;
    int *isSmallMap;
    int *smallCount;
    int *largeCount;
} localFlagArgs;

typedef struct
{
    int *arr;
    int *prefixSumArr;
    int size;
}calcPrefixSumArgs;

typedef struct
{
    int *arr;
    int count;
    int size;
}myqsortArgs;

void updateSpareThreads()
{
    pthread_mutex_lock(&spareThreadsLock);
    ++spareThreads;
    pthread_mutex_unlock(&spareThreadsLock);
}

void updateMyNumThreads(int *num_threads)
{
    pthread_mutex_lock(&spareThreadsLock);
    if(spareThreads > 0)
    {
        *num_threads += spareThreads;
        spareThreads  = 0;
    }
    pthread_mutex_unlock(&spareThreadsLock);
}

void updateQSortThreads()
{
    pthread_mutex_lock(&qSortThreadsLock);
    ++qSortThreadsNum;
    pthread_mutex_unlock(&qSortThreadsLock);
}

void updatePartitionThreads()
{
    pthread_mutex_lock(&partitionThreadsLock);
    ++partitionThreadsNum;
    pthread_mutex_unlock(&partitionThreadsLock);
}

int isArrPartitioned(int *a, int l, int r, int p)
{
    int i;
    for(i = l; i < p; i++)
    {
        if(a[i] > a[p])
                return 0;
    }
    for(i = p+1; i <= r; i++)
    {
        if(a[i] < a[p])
                return 0;
    }
    return 1;
}

/* Comparer */
int compare (const void * a, const void * b)
{
  return ( *(int*)a - *(int*)b );
}

int calcMedian(int *a, int len)
{
    qsort(a, len, sizeof(int), compare);
    return a[len/2];
}

int findPosition(int *sample, int *posSample, int median, int len)
{
    int i;
    for(i = 0; i < len; i++)
    {
        if(sample[i] == median)
            return posSample[i];
    }
    return -1;
}

/* Evaluate a sampled median of the list */
int getMedian(int *a, int left, int right, int *position)
{
    int i, len = right - left + 1;
    int *sampleArr, *copy;
    int *posSample;

    if(len > MEDIAN_SET_SIZE)
    {
        sampleArr = (int *) malloc(sizeof(int) * MEDIAN_SET_SIZE);
        copy = (int *) malloc(sizeof(int) * MEDIAN_SET_SIZE);
        posSample = (int *) malloc(sizeof(int) * MEDIAN_SET_SIZE);
        for(i = 0; i < MEDIAN_SET_SIZE; i++)
        {
            int index    = left + (rand() % len);
            sampleArr[i] = a[index];
            posSample[i] = index;
        }
        len = MEDIAN_SET_SIZE;
    }
    else
    {
        sampleArr = (int *) malloc(sizeof(int) * len);
        copy = (int *) malloc(sizeof(int) * len);
        posSample = (int *) malloc(sizeof(int) * len);
        memcpy(sampleArr, a + left, len * sizeof(int));
        for(i = left; i <= right; i++)
            posSample[i - left] = i;
    }
    memcpy(copy, sampleArr, len*sizeof(int));
    int median = calcMedian(sampleArr, len);
    *position  = findPosition(copy, posSample, median, len);
    
    free(sampleArr);
    free(copy);
    return median;
}

/* Call the libc qsort */
void *myqsort(void *arg)
{
    myqsortArgs *args = (myqsortArgs *)arg;
    qsort(args->arr, args->count, args->size, compare);
    free(arg);
    pthread_exit(0);
}

/* Calculates the prefix sum */
void *calcPrefixSum (void *arg)
{
    calcPrefixSumArgs *args = (calcPrefixSumArgs *) arg;
    int i = 0;
    args->prefixSumArr[i++] = 0;
    for(; i <= args->size; i++)
        args->prefixSumArr[i] = args->prefixSumArr[i - 1] + args->arr[i - 1];
    free(arg);
    pthread_exit(0);
}

void *moveElements(void *arg)
{
    moveElementsArgs *args = (moveElementsArgs *) arg;
    int i, j, smallPos, largePos, globalPos;
    smallPos = largePos = 0;
    j = args->left - args->globalLeft;
    for(i = args->left; i <= args->right; i++)
    {
        if(args->isSmallMap[j++])
            globalPos = args->globalLeft + args->smallPrefixSum + smallPos++;
        else
            globalPos = args->globalLeft + args->aLen - (args->totLarge - args->largePrefixSum) + largePos++;
        
        args->a[globalPos] = args->aCopy[i];
    }
    free(arg);
    pthread_exit(0);
}

/* In the current thread locally flags all the elements as small or large compared to the pivot */
void *localFlag (void *arg)
{
    localFlagArgs *args = (localFlagArgs *) arg;
    int i, j, smallCount, largeCount;
    smallCount = largeCount = 0;
    j = args->left - args->globalLeft;
    for(i = args->left; i <= args->right; i++)
    {
        /* Flag the element as smaller than pivot and increment the number of smaller elements in this thread */
        if(args->a[i] < args->pivot)
        {
            args->isSmallMap[j++] = 1;
            smallCount++;
        }
        /* Flag the element as larger than pivot and increment the number of larger elements in this thread */
        else
        {
            args->isSmallMap[j++] = 0;
            largeCount++;
        }
    }
    *(args->smallCount) = smallCount;
    *(args->largeCount) = largeCount;
    free(arg);
    pthread_exit(0);
}

/* Globally regarranges all the elements over all the threads */
int globalRearrange (int *a, int *aCopy, int left, int right, int *isSmallMap, int *smallCount, int *largeCount, int num_threads)
{
    int i, *smallPrefixSum, *largePrefixSum;
    int len       = right - left + 1;
    int blockSize = (int) ceil((double) len / num_threads);
    
    smallPrefixSum = largePrefixSum = NULL;
    smallPrefixSum = (int *) malloc(sizeof(int) * (num_threads + 1));
    largePrefixSum = (int *) malloc(sizeof(int) * (num_threads + 1));


    /* Over 2 threads */
    pthread_t *prefixThreads = (pthread_t *) malloc(sizeof(pthread_t) * 2);
   
    /* Fetch the prefix sum of the number of elements less than than the pivot */
    calcPrefixSumArgs *arg1 = (calcPrefixSumArgs *) malloc(sizeof(calcPrefixSumArgs));
    arg1->arr          = smallCount;
    arg1->prefixSumArr = smallPrefixSum;
    arg1->size         = num_threads;
    pthread_create(&prefixThreads[0], NULL, calcPrefixSum, (void *) arg1);
    
    /* Fetch the prefix sum of the number of elements greater than the pivot */
    calcPrefixSumArgs *arg2 = (calcPrefixSumArgs *) malloc(sizeof(calcPrefixSumArgs));
    arg2->arr          = largeCount;
    arg2->prefixSumArr = largePrefixSum;
    arg2->size         = num_threads;
    pthread_create(&prefixThreads[1], NULL, calcPrefixSum, (void *) arg2);
    
    pthread_join(prefixThreads[0], NULL);
    pthread_join(prefixThreads[1], NULL);

    /* Over all threads */
    pthread_t *myThreads = (pthread_t *) malloc(sizeof(pthread_t) * num_threads);
    int j = 0;
    /* Global rearrangement by calculating the global position with the help of the prefix sum arrays */
    for(i = 0; i < num_threads; i++)
    {
        int subLeft  = left + (blockSize * i);
        int subRight = subLeft + blockSize - 1 <= right ? subLeft + blockSize - 1 : right;
        if(subLeft <= subRight)
        {
            moveElementsArgs *args = (moveElementsArgs *) malloc(sizeof(moveElementsArgs));
            args->a              = a;
            args->aCopy          = aCopy;
            args->aLen           = len;
            args->globalLeft     = left;
            args->left           = subLeft;
            args->right          = subRight;
            args->isSmallMap     = isSmallMap;
            args->smallPrefixSum = smallPrefixSum[i];
            args->largePrefixSum = largePrefixSum[i];
            args->totLarge       = largePrefixSum[num_threads];
            pthread_create(&myThreads[i], NULL, moveElements, (void *) args);
            j++;
        }
    }
    
    for(i = 0; i < j; i++)
        pthread_join(myThreads[i], NULL);

    /* Get the point where the array is globally partitioned */
    /* DO NOT free up memory before evaluating partitionPoint */
    int partitionPoint = left + smallPrefixSum[num_threads];
    
    /* Free up memory */
    if(smallPrefixSum)
        free(smallPrefixSum);
    if(largePrefixSum)
        free(largePrefixSum);
    if(myThreads)
        free(myThreads);
    if(prefixThreads)
        free(prefixThreads);

    return partitionPoint;
}

/* Main function that performs local and global rearrangements and gives the job of sorting the sub-arrays down the tree */
void *pqsortHelper (void *arg)
{
    pqsortHelperArgs *args = (pqsortHelperArgs *) arg;
    int thread = args->thread;
    if(args->num_threads > 1)
    {
        int i, *isSmallMap, *smallCount, *largeCount;
        int len       = args->right - args->left + 1;
        int blockSize = (int) ceil((double) len / args->num_threads);

        isSmallMap = smallCount = largeCount = NULL;
        isSmallMap = (int *) malloc(sizeof(int) * len);
        smallCount = (int *) malloc(sizeof(int) * args->num_threads);
        largeCount = (int *) malloc(sizeof(int) * args->num_threads);

        /* Initialize smallCount and largeCount with zeros */
        memset(smallCount, 0, sizeof(int) * args->num_threads);
        memset(largeCount, 0, sizeof(int) * args->num_threads);

        int position;
        int pivot = getMedian(args->a, args->left, args->right, &position);
        
        int temp = args->a[args->left];
        args->a[args->left] = args->a[position];
        args->a[position] = temp;

        temp = args->aCopy[args->left];
        args->aCopy[args->left] = args->aCopy[position];
        args->aCopy[position] = temp;

        /* Over all threads */
        pthread_t *myThreads = (pthread_t *) malloc(sizeof(pthread_t) * args->num_threads);
        /* Locally rearrange the sub-arrays around the pivot */
        int j = 0;
        for(i = args->left; i <= args->right; i += blockSize)
        {
            int last  = i + blockSize - 1;
            int limit = last <= args->right ? last : args->right;
            if(i <= limit)
            {
                localFlagArgs *myArg = (localFlagArgs *) malloc(sizeof(localFlagArgs));
                myArg->a          = args->a;
                myArg->globalLeft = args->left;
                myArg->left       = i;
                myArg->right      = limit;
                myArg->pivot      = pivot;
                myArg->isSmallMap = isSmallMap;
                myArg->smallCount = smallCount + j;
                myArg->largeCount = largeCount + j;
                pthread_create(&myThreads[j], NULL, localFlag, (void *) myArg);
                j++;
            }
        }
        
        for(i = 0; i < j; i++)
            pthread_join(myThreads[i], NULL);

        /* Globally rearrange the sub-arrays around the pivot */
        int partitionPoint = globalRearrange(args->a, args->aCopy, args->left, args->right, isSmallMap, smallCount, largeCount, args->num_threads);
        int numOfLeftNums  = partitionPoint - args->left;
        int numOfRightNums = args->right - partitionPoint;
        
        /* Free up memory */
        if(isSmallMap)
            free(isSmallMap);
        if(smallCount)
            free(smallCount);
        if(largeCount)
            free(largeCount);
        if(myThreads)
            free(myThreads);

        int numThreads = args->num_threads;
        updateMyNumThreads(&numThreads);
        if(numThreads > args->num_threads)
            args->num_threads = numThreads;

        /* Calculate the number of threads to work on the left and right paritions */
        int numLeftThreads  = (int) ((double) numOfLeftNums / globalNbyP + 0.5);//(int) ceil((double)(args->num_threads * (double) numOfLeftNums / len));
        int numRightThreads = (int) ((double) numOfRightNums / globalNbyP + 0.5);//args->num_threads - numLeftThreads;
        
        /* Create a copy of the current array */
        memcpy(args->aCopy + args->left, args->a + args->left, len * sizeof(int));

        /* Sort the left parition of the array */
        if(numLeftThreads > 1 && numOfLeftNums > globalNbyP)
        {
            pqsortHelperArgs *argL = (pqsortHelperArgs *) malloc(sizeof(pqsortHelperArgs));
            argL->a           = args->a;
            argL->aCopy       = args->aCopy;
            argL->left        = args->left;
            argL->right       = partitionPoint - 1;
            argL->num_threads = numLeftThreads;
            argL->thread      = 1;
            updatePartitionThreads();
            pthread_create(&partitionThreads[partitionThreadsNum],NULL, pqsortHelper, (void *) argL);
        }
        else
        {
            updateSpareThreads();
            myqsortArgs *argSort = (myqsortArgs *) malloc(sizeof(myqsortArgs));
            argSort->arr   = args->a + args->left;
            argSort->count = partitionPoint - args->left;
            argSort->size  = sizeof(int);
            updateQSortThreads();
            pthread_create(&qSortThreads[qSortThreadsNum], NULL, myqsort, (void *) argSort);
        }
        
        /* Sort the right parition of the array */
        if(numRightThreads > 1 && len-numOfLeftNums > globalNbyP)
        {
            pqsortHelperArgs *argR = (pqsortHelperArgs *) malloc(sizeof(pqsortHelperArgs));
            argR->a           = args->a;
            argR->aCopy       = args->aCopy;
            argR->left        = partitionPoint + 1;
            argR->right       = args->right;
            argR->num_threads = numRightThreads;
            argR->thread      = 1;
            updatePartitionThreads();
            pthread_create(&partitionThreads[partitionThreadsNum], NULL, pqsortHelper, (void *) argR);
        }
        else
        {
            updateSpareThreads();
            myqsortArgs *argSort = (myqsortArgs *) malloc(sizeof(myqsortArgs));
            argSort->arr   = args->a + partitionPoint + 1;
            argSort->count = args->right - partitionPoint;
            argSort->size  = sizeof(int);
            updateQSortThreads();
            pthread_create(&qSortThreads[qSortThreadsNum], NULL, myqsort, (void *) argSort);
        }
    }
    else
        qsort(args->a, args->right - args->left + 1, sizeof(int), compare);

    free(arg);
    if(thread)
        pthread_exit(0);
}

/* The function to sort an array of integers of size num_elements to be sorted using num_threads threads */
int *pqsort(int *input, int num_elements, int num_threads)
{
    pqsortHelperArgs *arg = (pqsortHelperArgs *) malloc(sizeof(pqsortHelperArgs));
    int *inputCopy = (int *) malloc(sizeof(int) * num_elements);
    memcpy(inputCopy, input, num_elements * sizeof(int));

    globalNbyP = num_elements / num_threads;

    pthread_mutex_init(&qSortThreadsLock, NULL);
    pthread_mutex_init(&spareThreadsLock, NULL);
    pthread_mutex_init(&partitionThreadsLock, NULL);

    // Construct the struct for passing to pqsortHelper()
    arg->a           = input;
    arg->aCopy       = inputCopy;
    arg->left        = 0;
    arg->right       = num_elements - 1;
    arg->num_threads = num_threads;
    arg->thread      = 0;
    pqsortHelper((void *) arg);

    int i;
    for(i = 0; i <= partitionThreadsNum; i++)
        pthread_join(partitionThreads[i], NULL);
    for(i = 0; i <= qSortThreadsNum; i++)
        pthread_join(qSortThreads[i], NULL);

    return input;
}
