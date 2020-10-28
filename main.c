#include <stdio.h>
#include <stdlib.h>

#include "threads.h"

/*
#define THREAD_COUNT 4

// waste some time
void *count(void *arg) {
    unsigned long int c = (unsigned long int)arg;
    int i;
    for (i = 0; i < c; i++) {
        if ((i % 1000000) == 0) {
            printf("tid: 0x%x Just counted to %d of %ld\n", (unsigned int)pthread_self(), i, c);
        }
    }
    return arg;
}

int main(int argc, char **argv) {
    pthread_t threads[THREAD_COUNT];
    int i;
    unsigned long int cnt = 10000000;

    //create THREAD_COUNT threads
    for (i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, count, (void *)((i + 1) * cnt));
    }

    //join all threads ... not important for proj2
    for (i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    // But we have to make sure that main does not return before
    // the threads are done... so count some more...
    count((void *)(cnt * (THREAD_COUNT + 1)));
    return 0;
}
*/

#define PRODUCER_COUNT 2
#define CONSUMER_COUNT 2
#define N 8

sem_t empty;
sem_t full;
sem_t mutex;
int i;         // for loop
int item = 0;  // used instead of buffer
int limit = 0;

static void hline(char* str) { printf("\n----------8<-------------[ %s %x ]----------\n", str, (unsigned)pthread_self()); }

void* producer(void* args) {  // must have this prototype for pthread_create
    while (limit < N * 10) {
        hline("PRODUCER");
        // printf("P: empty.down() called");
        sem_wait(&empty);  // empty.down()
        // printf("P: mutex.down() called");
        sem_wait(&mutex);  // mutex.down()

        printf("Producer %x Just added an item to %d (Now: %d)\n", (unsigned)pthread_self(), item, item + 1);
        ++item;

        // printf("P: mutex.up() called");
        sem_post(&mutex);  // mutex.up()
        // printf("P: full.up() called");
        sem_post(&full);  // full.up()
        ++limit;
    }
    return NULL;
}

void* consumer(void* args) {
    while (limit < N * 10) {
        hline("CONSUMER");
        // printf("C: full.down() called");
        sem_wait(&full);  // full.down()
        // printf("C: mutex.down() called");
        sem_wait(&mutex);  // mutex.down()

        printf("Consumer %x Just removed an item from %d (Now: %d)\n", (unsigned)pthread_self(), item, item - 1);
        --item;  // decrement item

        // printf("C: mutex.up() called");
        sem_post(&mutex);  //mutex.up()
        // printf("C: empty.up() called");
        sem_post(&empty);  // empty.up()
        ++limit;
    }
    return NULL;
}

int main(int argc, char** argv) {
    pthread_t threads[PRODUCER_COUNT + CONSUMER_COUNT];
    void* value_ptr[PRODUCER_COUNT + CONSUMER_COUNT];

    for (i = 0; i < PRODUCER_COUNT + CONSUMER_COUNT; ++i)
        value_ptr[i] = malloc(sizeof(int*));

    sem_init(&empty, 0, N);
    sem_init(&full, 0, 0);
    sem_init(&mutex, 0, 1);

    // create consumers and producers
    for (i = 0; i < PRODUCER_COUNT; ++i)
        pthread_create(&threads[i], NULL, producer, NULL);

    for (i = PRODUCER_COUNT; i < PRODUCER_COUNT + CONSUMER_COUNT; ++i)
        pthread_create(&threads[i], NULL, consumer, NULL);

    // join to ensure main does not exit
    for (i = 0; i < PRODUCER_COUNT; ++i)
        pthread_join(threads[i], value_ptr[i]);

    for (i = PRODUCER_COUNT; i < PRODUCER_COUNT + CONSUMER_COUNT; ++i)
        pthread_join(threads[i], value_ptr[i]);

    // print return values
    for (i = 0; i < PRODUCER_COUNT; ++i)
        printf("Producer %d exited with return code %d \n", i + 1, *(int*)value_ptr[i]);

    for (i = PRODUCER_COUNT; i < PRODUCER_COUNT + CONSUMER_COUNT; ++i)
        printf("Consumer %d exited with return code %d \n", i + 1, *(int*)value_ptr[i]);

    return 0;
}
