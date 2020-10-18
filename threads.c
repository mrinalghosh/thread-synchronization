#include "threads.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ec440threads.h"

/*
TODO: 
lock, unlock - DONE - put in the right places
pthread_join - DONE 
semaphores -    
*/

int i;  // iteration variable
int thread_count = 0;
TCB *head = NULL;
TCB *current = NULL;
sigset_t mask;

// static void hline(void) { printf("----------8<-------------[ cut here ]----------\n"); }

static TCB *traverse(int n) {
    TCB *temp = head;
    for (i = 0; i < n; ++i)
        temp = temp->next;
    return temp;
}

static void pthread_exit_wrapper() {
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
    mt->status = RUNNING;
    mt->id = (pthread_t)thread_count;
    mt->exit_code = NULL;
    mt->exit_addr = NULL;

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

int sem_init(sem_t *sem, int pshared, unsigned value) {
    // pshared const 0
    semaphore *ns = (semaphore *)calloc(sizeof(semaphore));
    ns->value = value;
    ns->queue = NULL;
    ns->init = true;
    sem->__align = (long)ns;
    return 0;  // if successful
}

int sem_wait(sem_t *sem) {}

int sem_post(sem_t *sem) {}

int sem_destroy(sem_t *sem) {}

void scheduler(int signum) {
    // hline();
    lock();

    if (!setjmp(current->buf)) {
        do {
            if (current->status == BLOCKED && traverse(current->dep)->status == EXITED) {
                if (current->value_ptr && traverse(current->dep)->exit_addr)
                    *(current->value_ptr) = *(traverse(current->dep)->exit_addr);
                break;
            }

            if (current->status == RUNNING)
                current->status = READY;
            current = current->next;

        } while (current->status == EXITED || current->status == BLOCKED);

        current->status = RUNNING;
        longjmp(current->buf, 1);
    }

    unlock();
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
    new_thread->status = READY;
    new_thread->id = (pthread_t)thread_count;
    *thread = (pthread_t)thread_count;  // write to arg thread - used in pthread_join

    new_thread->exit_code = NULL;
    new_thread->exit_addr = NULL;

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
    lock();
    current->status = EXITED;
    current->exit_addr = &value_ptr;
    free(current->stack - STACKSIZE + 8);
    unlock();

    scheduler(SIGALRM);
    __builtin_unreachable();
}

pthread_t pthread_self(void) { return current->id; }

void lock(void) {  // needs to be called to protect accesses to global data structures
    sigemptyset(&mask);
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
    if (traverse((unsigned)thread)->status == EXITED) {
        if (value_ptr)
            *value_ptr = *(traverse((unsigned)thread)->exit_addr);
    } else {  // ante may be READY - can't be RUNNING - may be BLOCKED itself
        lock();
        current->dep = thread;
        if (value_ptr)
            current->value_ptr = value_ptr;
        current->status = BLOCKED;
        unlock();
        scheduler(SIGALRM);
    }

    return 0;
}
