#include "threads.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ec440threads.h"

int i;  // iteration variable
int thread_count = 0;
TCB *head = NULL;
TCB *current = NULL;
sigset_t mask;

static void hline(void) { printf("\n->->->->->->->->-A L A R M->->->->->->->->->-\n\n"); }

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
    mt->exit_address = NULL;

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

int sem_init(sem_t *sem, int pshared, unsigned value) {  // pshared const 0
    semaphore *ns = (semaphore *)calloc(1, sizeof(semaphore));
    ns->value = value;
    ns->queue = NULL;
    ns->init = true;
    sem->__align = (long)ns;
    return 0;  // if successful - error codes?
}

int sem_wait(sem_t *sem) {
    lock();
    semaphore *s = (semaphore *)sem->__align;  // dereference semaphore from address in __align
    // printf("semaphore.down(%d)\n", s->value);
    if (s->value == 0) {            //TODO: block when semaphore val == 0 until possible to decrement
        current->status = BLOCKED;  //TODO: might need a different status when not blocked by pthread_join?

        wait_queue *nq = (wait_queue *)calloc(1, sizeof(wait_queue));  // allocate new queue

        nq->tcb = current;  // point to current TCB that is now blocked
        nq->next = NULL;    // at the end of current queue of semaphores

        if (s->queue == NULL) {  // NULL - nothing in sem's queue - put in first position
            s->queue = nq;       // make first term in queue
        } else {                 // if queue exists - put at end of sem queue
            wait_queue *temp = s->queue;
            while (temp->next != NULL) {
                temp = temp->next;  // increment to end of queue
            }
            temp->next = nq;
            nq->next = NULL;
        }
        unlock();            // since not returning to function
        scheduler(SIGALRM);  // SCHEDULE NOW - Camden
    } else {
        --(s->value);  // DOESN'T MATTER - SCHEDULES OUT OF FUNCTION ABOVE
    }
    unlock();
    return 0;
}

int sem_post(sem_t *sem) {
    lock();
    semaphore *s = (semaphore *)sem->__align;
    // CURRENT THREAD CAN'T be blocked - it called sem_post in the first place
    // printf("semaphore.up(%d)\n", s->value);
    if (!s->value) {  // semaphore value = 0
        // will become greater than zero now - another thread blocked in sem_wait can be woken up
        // increment unless another thread is found to unblock

        if (s->queue != NULL) {  // assuming queue is FIFO - first thread blocked by sem resumes

            //TODO: actively or passively 'wake up' thread first waiting thread
            s->queue->tcb->prev->next = s->queue->tcb->next;
            s->queue->tcb->next->prev = s->queue->tcb->prev;

            current->next->prev = s->queue->tcb;  // current->next->prev = target
            s->queue->tcb->prev = current;        // target->prev = current
            s->queue->tcb->next = current->next;  // target->next = current->next
            current->next = s->queue->tcb;        // current->next = target

            s->queue->tcb->status = READY;

            wait_queue *temp = s->queue;  // point to first wait_queue
            s->queue = s->queue->next;    // remove and free first in queue
            free(temp);
            // scheduler(SIGALRM); // not calling scheduler otherwise runs for just one cycle

        } else {           // NOT WAKING UP THREAD - can increment and unlock
            ++(s->value);  // increments semaphore value - unless another thread is woken up!!!
            unlock();
        }
    }
    // shouldn't schedule here
    return 0;
}

int sem_destroy(sem_t *sem) {
    lock();
    semaphore *s = (semaphore *)sem->__align;
    wait_queue *temp = s->queue;
    // free semaphore queues
    if (temp != NULL) {  // queue not NULL
        if (temp->next != NULL) {
            temp = temp->next;
            while (temp->next != NULL) {
                free(s->queue);         // free first in wait_queue
                s->queue = temp;        // restore value of queue from temp
                temp = s->queue->next;  // temp points to next queue element, s->queue points to current element
            }
            free(s->queue);  // finally dobby is a free elf
            //TODO: make sure this works with single element case
        } else {
            free(s->queue);
        }
    }
    free(s);
    unlock();
    return 0;
}

void scheduler(int signum) {
    hline();
    lock();

    if (!setjmp(current->buf)) {
        do {
            if (current->status == BLOCKED && traverse(current->ante)->status == EXITED) {
                if (current->value_ptr && traverse(current->ante)->exit_address)
                    *(current->value_ptr) = *(traverse(current->ante)->exit_address);
                break;
            }
            if (current->status == RUNNING) {
                current->status = READY;
            }
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

    new_thread->exit_address = NULL;

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
    current->exit_address = &value_ptr;
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
            *value_ptr = *(traverse((unsigned)thread)->exit_address);
    } else {  // ante may be READY - can't be RUNNING - may be BLOCKED itself
        lock();
        current->ante = thread;
        if (value_ptr)
            current->value_ptr = value_ptr;
        current->status = BLOCKED;
        unlock();
        scheduler(SIGALRM);
    }

    return 0;
}
