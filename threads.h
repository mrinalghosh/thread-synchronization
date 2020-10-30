#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include <semaphore.h>
#include <setjmp.h>
#include <stdbool.h>

#define QUOTA 50000
#define STACKSIZE 32767
#define MAXTHREADS 128

#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC 7

typedef enum State {
    RUNNING,
    READY,
    EXITED,
    BLOCKED,
    SEMAPHORE_BLOCKED,
} State;

typedef struct TCB {
    pthread_t id;
    jmp_buf buf;
    void *stack;
    State status;
    struct TCB *next;
    struct TCB *prev;
    void *exit_value;
    pthread_t ante;
} TCB;

typedef struct wait_queue {
    TCB *tcb;
    struct wait_queue *next;
} wait_queue;

typedef struct semaphore {
    unsigned value;
    struct wait_queue *queue;
    bool init;
} semaphore;

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
void pthread_exit(void *value_ptr);
pthread_t pthread_self(void);
void scheduler(int signum);

void lock(void);
void unlock(void);

int pthread_join(pthread_t thread, void **value_ptr);

int sem_init(sem_t *sem, int pshared, unsigned value);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_destroy(sem_t *sem);

#endif /* THREADS_H */