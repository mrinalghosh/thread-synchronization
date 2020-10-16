#include "threads.h"

#include <signal.h>
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

int i;  // iteration variable
int thread_count = 0;
TCB *head = NULL;
TCB *current = NULL;
TCB *ante = NULL;
sigset_t mask;  // sigmask to hide SIGALRM when locking a thread

static TCB *traverse(int n) {
    TCB *temp = head;
    for (i = 0; i < n; ++i)
        temp = temp->next;
    return temp;
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
    mt->status = RUNNING;
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

void scheduler(int signum) {
    // TODO: where to put lock in scheduler?
    if (!setjmp(current->buf)) {
        do {
            if (current->status == BLOCKED && traverse(current->dep)->status == EXITED)  // current blocked but dependency has finished - CONTINUE RUNNING NOW
                break;
            if (current->status == RUNNING)
                current->status = READY;
            current = current->next;
        } while (current->status == EXITED || current->status == BLOCKED);

        current->status = RUNNING;
        longjmp(current->buf, 1);
    }
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
    current->exit_code = value_ptr;        // save the return value for pthread_join
    free(current->stack - STACKSIZE + 8);  // TODO: free all TCB resources beside exit value - collected in pthread_join
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
    // postpone execution of calling thread till target thread terminates
    // thread should be numerically away from head - just need to move up by that many in doubly linked list - traverse()
    // IT WORKS - PTHREAD_JOIN isn't called again by the same thread until the last call returns. Therefore a list of dependencies doesn't build up - more like a mailbox

    ante = traverse((unsigned)thread);

    // printf("BLOCKING THREAD %d TILL THREAD %d is complete\n", (unsigned)current->id, (unsigned)ante->id);

    if (ante->status == EXITED) {  // thread to join has exited
        value_ptr = &ante->exit_code;
        return 0;  // TODO: 0=successful - otherwise choose and return error values
    } else {       // ante may be READY - can't be RUNNING - may be BLOCKED itself
        lock();
        current->dep = thread;
        current->status = BLOCKED;
        unlock();
        scheduler(SIGALRM);
    }

    // use __builtin_unreachable() since might go to scheduler?
    return 0;
}
