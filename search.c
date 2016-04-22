// ------------------------ HEADERS ------------------------
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>

// ------------------------ CONSTANTS ------------------------
#define BASE 10
#define LOWER 0
#define UPPER 999

// ------------------------ DATA DEFINITIONS ------------------------
typedef struct {
    int size;                   // number of elements to be searched
    int offset;                 // index at which searching is to begin
    int *counter;               // counter to be incremented when num_search is found
    int *array;                 // array to be searched
    int num_search;             // number to be searched for
    pthread_mutex_t *lock;      // the mutex lock
    pthread_barrier_t *barrier; // the barrier
} params_t;

// ------------------------ FUNCTION DECLARATIONS ------------------------
bool extract_number(char *, int *);
void print_error_and_exit(char *);
int rando(int);
void *run(void *);

// ------------------------ FUNCTION DEFINITIONS ------------------------
int main(int argc, char **argv) {       
    if(argc < 5) {
        print_error_and_exit(argv[0]);
    }
    
    // ------------------------ PARSE ARGUMENTS ------------------------
    int num_search, size, num_omit, num_threads;

    if(!extract_number(argv[1], &size) || !extract_number(argv[2], &num_omit)
            || !extract_number(argv[3], &num_threads) || !extract_number(argv[4], &num_search)) {
        print_error_and_exit(argv[0]);
    } 
    
    if(num_omit < LOWER || num_omit > UPPER || num_search < LOWER || num_search > UPPER) {
        print_error_and_exit(argv[0]);
    }
    
    if(size < 0 || num_threads > size) {
        print_error_and_exit(argv[0]);
    }

    // ------------------------ INITIALIZE MUTEX & BARRIER ------------------------
    pthread_mutex_t lock;
    pthread_barrier_t barrier; 

    if(pthread_mutex_init(&lock, NULL)) {
        fprintf(stderr, "Mutex initialization failed.");
        exit(EXIT_FAILURE);
    }
    if(pthread_barrier_init(&barrier, NULL, num_threads + 1)) {
        fprintf(stderr, "Barrier initialization failed.");
        exit(EXIT_FAILURE);
    }
    
    // ------------------------ CREATE THREADS ------------------------
    pthread_t threads[num_threads];
    params_t params[num_threads];
    int counter = 0;
    int *array = (int *) calloc(sizeof(int), size);
    if(array == NULL) {
        fprintf(stderr, "Couldn't allocate memory for array.");
        exit(EXIT_FAILURE);
    }
    
    int partition_size = size / num_threads;
    int last_partition = partition_size + (size % num_threads);

    for(int i = 0; i < num_threads; i++) {
        params[i].array = array;
        params[i].num_search = num_search;
        params[i].lock = &lock;
        params[i].barrier = &barrier;
        params[i].counter = &counter;
        params[i].offset = partition_size * i;
        params[i].size = (i == num_threads - 1) ? last_partition : partition_size;
        
        if(pthread_create(&threads[i], NULL, &run, (void *) &params[i])) {
            fprintf(stderr, "Couldn't create thread.");
            exit(EXIT_FAILURE);
        }
    }

    // ------------------------ GENERATE RANDOM DATA ------------------------
    srand(time(NULL));
    for(int i = 0; i < size; i++) {
        array[i] = rando(num_omit);
    }

    // ------------------------ START PARALLEL SEARCH ------------------------
    int rc = pthread_barrier_wait(&barrier);
    if(rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
        fprintf(stderr, "Couldn't wait at barrier.");
        exit(EXIT_FAILURE);
    }

    // ------------------------ WAIT FOR THREADS TO FINISH ------------------------
    for(int i = 0; i < num_threads; i++) {
        if(pthread_join(threads[i], NULL)) {
            fprintf(stderr, "Couldn't join thread.");
            exit(EXIT_FAILURE);
        }
    }
    
    // ------------------------ FREE UP RESOURCES ------------------------
    free(array);
    array = NULL;
    if(pthread_mutex_destroy(&lock)) {
        fprintf(stderr, "Couldn't destroy mutex.");
        exit(EXIT_FAILURE);
    }
    if(pthread_barrier_destroy(&barrier)) {
        fprintf(stderr, "Couldn't destroy barrier.");
        exit(EXIT_FAILURE);
    }
    
    // ------------------------ DISPLAY RESULTS ------------------------
    printf("\nArray size: %d\n", size);
    printf("Number to omit: %d\n", num_omit);
    printf("Number of threads: %d\n", num_threads);
    printf("Number to search for: %d\n", num_search);
    printf("\n%d was found %d times in this array.\n", num_search, counter);
    
    printf("\n");
    return (EXIT_SUCCESS);
}

/*
 * Parse a number from given string, and store in given location. Return true if parsing was successful. False, otherwise.
 */
bool extract_number(char *str, int *res) {
    char *endptr;
    
    errno = 0;
    long val = strtol(str, &endptr, BASE);
    if((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
        return false;
    }
    if(endptr == str) {
        return false;
    }
    if(val > INT_MAX || val < INT_MIN) {
        return false;
    }
    if(res != NULL) {
        *res = (int) val;
    }
    return true;
}

/*
 * Given the name of this program, print out the usage error message, and exit.
 */
void print_error_and_exit(char *progname) {
    fprintf(stderr, "\nUsage: %s <array size> <number to omit> <number of threads> <number to search for>\n\n", progname);
    exit(EXIT_FAILURE);
}

/*
 * Given an integer, say x, produce a random number between 0-999 except x.
 */
int rando(int omit) {
    int val = rand() % (UPPER + 1);
    return (val != omit) ? val : rando(omit);
}

/*
 * Entry-point for worker threads. Each thread searches for num_search over size elements starting from offset. 
 */
void *run(void *arg) {
    params_t *input = (params_t *) arg;

    // ------------------------ WAIT AT BARRIER ------------------------
    int rc = pthread_barrier_wait(input->barrier);
    if(rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
        fprintf(stderr, "Couldn't wait at barrier.");
        exit(EXIT_FAILURE);
    }

    // ------------------------ BEGIN LINEAR SEARCH ------------------------
    for(int i = input->offset; i < (input->offset + input->size); i++) {
        if(input->array[i] == input->num_search) {
            pthread_mutex_lock(input->lock);
            *(input->counter) = *(input->counter) + 1;
            pthread_mutex_unlock(input->lock);
        }
    }
    return NULL;
}
