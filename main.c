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

#define PRODUCER_COUNT 1
#define CONSUMER_COUNT 1
#define N 8

sem_t empty;
sem_t full;
sem_t mutex;
int i;         // for loop
int item = 0;  // used instead of buffer

static void hline(char* str) { printf("\n----------8<-------------[ %s %x ]----------\n", str, (unsigned)pthread_self()); }

void* producer(void* args) {  // must have this prototype for pthread_create
    while (true) {
        hline("PRODUCER");
        printf("P: empty.down() called\n");
        sem_wait(&empty);  // empty.down()
        printf("P: mutex.down() called\n");
        sem_wait(&mutex);  // mutex.down()

        printf("Producer %x Just added an item to %d (Now: %d)\n", (unsigned)pthread_self(), item, item + 1);
        ++item;

        printf("P: mutex.up() called\n");
        sem_post(&mutex);  // mutex.up()
        printf("P: full.up() called\n");
        sem_post(&full);   // full.up()
    }
}

void* consumer(void* args) {
    while (true) {
        hline("CONSUMER");
        printf("C: full.down() called\n");
        sem_wait(&full);   // full.down()
        printf("C: mutex.down() called\n");
        sem_wait(&mutex);  // mutex.down()

        printf("Consumer %x Just removed an item from %d (Now: %d)\n", (unsigned)pthread_self(), item, item - 1);
        --item;  // decrement item

        printf("C: mutex.up() called\n");
        sem_post(&mutex);  //mutex.up()
        printf("C: empty.up() called\n");
        sem_post(&empty);  // empty.up()
    }
}

int main(int argc, char** argv) {
    pthread_t threads[PRODUCER_COUNT + CONSUMER_COUNT];

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
        pthread_join(threads[i], NULL);

    for (i = PRODUCER_COUNT; i < PRODUCER_COUNT + CONSUMER_COUNT; ++i)
        pthread_join(threads[i], NULL);

    return 0;
}
