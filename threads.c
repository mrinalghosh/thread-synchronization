#include "threads.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ec440threads.h"

/*
TODO: 
lock, unlock - DONE
pthread_join
semaphores
*/


int thread_count = 0;
TCB *head = NULL;
TCB *current = NULL;

sigset_t mask;  // sigmask to hide SIGALRM when locking a thread
sigemptyset(&mask);

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

static void pthread_exit_wrapper() {
    /* load return value of thread's start function -> res variable -> call pthread_exit */
    unsigned long int res;
    asm("movq %%rax, %0\n"
        : "=r"(res));
    pthread_exit((void *)res);
}

static void thread_init(void) {
    TCB *mt = (TCB *)calloc(1, sizeof(TCB));

    head = mt;
    current = mt;
    mt->next = mt;
    mt->prev = mt;
    mt->state = RUNNING;
    mt->id = (pthread_t)thread_count;
    mt->exit_code = NULL;

    setjmp(mt->buf);

    struct sigaction act;
    memset(&act, '\0', sizeof(act));
    act.sa_handler = scheduler;
    act.sa_flags = SA_NODEFER;
    if (sigaction(SIGALRM, &act, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }

    ualarm(QUOTA, QUOTA);
    ++thread_count;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    if (!thread_count)
        thread_init();

    if (thread_count >= MAXTHREADS) {
        perror("maximum threads");
        exit(1);
    }

    TCB *new_thread = (TCB *)calloc(1, sizeof(TCB));

    new_thread->prev = head->prev;
    head->prev->next = new_thread;
    head->prev = new_thread;
    new_thread->next = head;
    new_thread->state = READY;
    new_thread->id = (pthread_t)thread_count;
    new_thread->exit_code = NULL;

    setjmp(new_thread->buf);

    new_thread->stack = calloc(1, STACKSIZE);
    new_thread->stack = new_thread->stack + STACKSIZE - 8;
    *(unsigned long *)new_thread->stack = (unsigned long)pthread_exit_wrapper;  // replaced pthread_exit

    new_thread->buf[0].__jmpbuf[JB_R12] = (unsigned long)start_routine;
    new_thread->buf[0].__jmpbuf[JB_R13] = (unsigned long)arg;
    new_thread->buf[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long)start_thunk);
    new_thread->buf[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long)new_thread->stack);

    ++thread_count;
    return 0;
}

void pthread_exit(void *value_ptr) {
    current->state = EXITED;
    current->exit_code = value_ptr;  // save the return value for later - in pthread_join
   
    // TODO: free all resources once exit value is collected in pthread_join

    scheduler(SIGALRM);
    __builtin_unreachable();
}

pthread_t pthread_self(void) { return current->id; }

void lock(void) {  // needs to be called to protect accesses to global data structures
    sigaddset(&mask, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        perror("sigprocmask");
        exit(1);
    }
}

void unlock(void) {
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) < 0) {  // or SIG_SETMASK
        perror("sigprocmask");
        exit(1);
    }
}

int pthread_join(pthread_t thread, void **value_ptr) {
    // postpone execution of calling thread till target thread terminates
    // does only main(head) call pthread_join? - can add a bitmask of the different threads that it's waiting for

    // put return value from pthread_exit into value ptr
}
