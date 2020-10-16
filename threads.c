#include "threads.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ec440threads.h"

int thread_count = 0;
TCB *head = NULL;
TCB *current = NULL;

void scheduler(int signum) {
    if (!setjmp(current->buf)) {
        do {
            if (current->state == RUNNING)
                current->state = READY;
            current = current->next;
        } while (current->state == EXITED);
        current->state = RUNNING;
        longjmp(current->buf, 1);
    }
}

static void thread_init(void) {
    TCB *mt = (TCB *)calloc(1, sizeof(TCB));

    head = mt;
    current = mt;
    mt->next = mt;
    mt->prev = mt;

    mt->state = RUNNING;
    mt->id = (pthread_t)thread_count;

    setjmp(mt->buf);

    ++thread_count;

    struct sigaction act;
    memset(&act, '\0', sizeof(act));
    act.sa_handler = scheduler;
    act.sa_flags = SA_NODEFER;

    if (sigaction(SIGALRM, &act, NULL) < 0) {
        perror("SIGACTION");
        exit(1);
    }

    ualarm(QUOTA, QUOTA);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    if (!thread_count) {
        thread_init();
    }

    if (thread_count >= MAXTHREADS) {
        perror("max threads reached");
        exit(1);
    }

    TCB *new_thread = (TCB *)calloc(1, sizeof(TCB));

    new_thread->prev = head->prev;
    head->prev->next = new_thread;
    head->prev = new_thread;
    new_thread->next = head;

    new_thread->id = (pthread_t)thread_count;
    *thread = new_thread->id;

    new_thread->stack = calloc(1, STACKSIZE);

    setjmp(new_thread->buf);
    new_thread->state = READY;
    new_thread->stack = new_thread->stack + STACKSIZE - 8;
    *(unsigned long *)new_thread->stack = (unsigned long)pthread_exit;

    new_thread->buf[0].__jmpbuf[JB_R12] = (unsigned long)start_routine;
    new_thread->buf[0].__jmpbuf[JB_R13] = (unsigned long)arg;
    new_thread->buf[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long)start_thunk);
    new_thread->buf[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long)new_thread->stack);

    ++thread_count;
    return 0;
}

void pthread_exit(void *value_ptr) {
    current->state = EXITED;
    scheduler(SIGALRM);
    __builtin_unreachable();
}

pthread_t pthread_self(void) { return current->id; }