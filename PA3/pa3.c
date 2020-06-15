/**********************************************************************
 * Copyright (c) 2020
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include "types.h"
#include "locks.h"
#include "atomic.h"
#include "list_head.h"

/*********************************************************************
 * Spinlock implementation
 *********************************************************************/
struct spinlock {
	int held;

};

/*********************************************************************
 * init_spinlock(@lock)
 *
 * DESCRIPTION
 *   Initialize your spinlock instance @lock
 */
void init_spinlock(struct spinlock *lock)
{
	lock->held = 0;
	return;
}

/*********************************************************************
 * acqure_spinlock(@lock)
 *
 * DESCRIPTION
 *   Acquire the spinlock instance @lock. The returning from this
 *   function implies that the calling thread grapped the lock.
 *   In other words, you should not return from this function until
 *   the calling thread gets the lock.
 */
void acquire_spinlock(struct spinlock *lock)
{
	while(compare_and_swap(&lock->held, 0, 1));

	return;
}

/*********************************************************************
 * release_spinlock(@lock)
 *
 * DESCRIPTION
 *   Release the spinlock instance @lock. Keep in mind that the next thread
 *   can grap the @lock instance right away when you mark @lock available;
 *   any pending thread may grap @lock right after marking @lock as free
 *   but before returning from this function.
 */
void release_spinlock(struct spinlock *lock)
{
	lock->held = 0;
	return;
}


/********************************************************************
 * Blocking mutex implementation
 ********************************************************************/
struct thread {
	pthread_t pthread;
	struct list_head list;
};

struct mutex {
	int count;
	int lock;
	struct list_head waiter;
};

/*********************************************************************
 * init_mutex(@mutex)
 *
 * DESCRIPTION
 *   Initialize the mutex instance pointed by @mutex.
 */


void init_mutex(struct mutex *mutex)
{	
	INIT_LIST_HEAD(&mutex->waiter);
	mutex->count = 1;
	mutex->lock = 0;
	return;
}

/*********************************************************************
 * acquire_mutex(@mutex)
 *
 * DESCRIPTION
 *   Acquire the mutex instance @mutex. Likewise acquire_spinlock(), you
 *   should not return from this function until the calling thread gets the
 *   mutex instance. But the calling thread should be put into sleep when
 *   the mutex is acquired by other threads.
 *
 * HINT
 *   1. Use sigwaitinfo(), sigemptyset(), sigaddset(), sigprocmask() to
 *      put threads into sleep until the mutex holder wakes up
 *   2. Use pthread_self() to get the pthread_t instance of the calling thread.
 *   3. Manage the threads that are waiting for the mutex instance using
 *      a custom data structure containing the pthread_t and list_head.
 *      However, you may need to use a spinlock to prevent the race condition
 *      on the waiter list (i.e., multiple waiters are being inserted into the 
 *      waiting list simultaneously, one waiters are going into the waiter list
 *      and the mutex holder tries to remove a waiter from the list, etc..)
 */




void acquire_mutex(struct mutex *mutex)
{	
	sigset_t set;
	siginfo_t info;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);	

	pthread_sigmask(SIG_BLOCK, &set, NULL);
	while(compare_and_swap(&mutex->lock, 0, 1));
	
	if(mutex->count > 0){
		mutex->count--;	
		mutex->lock = 0;
		pthread_sigmask(SIG_UNBLOCK, &set, NULL);
		return;
	}
	struct thread t;
	t.pthread = pthread_self();
	list_add_tail(&t.list, &mutex->waiter);
	mutex->lock = 0;
	sigwaitinfo(&set, &info);
	pthread_sigmask(SIG_UNBLOCK, &set, NULL);
	return;
}


/*********************************************************************
 * release_mutex(@mutex)
 *
 * DESCRIPTION
 *   Release the mutex held by the calling thread.
 *
 * HINT
 *   1. Use pthread_kill() to wake up a waiter thread
 *   2. Be careful to prevent race conditions while accessing the waiter list
 */
void release_mutex(struct mutex *mutex)
{
	sigset_t set;
	siginfo_t info;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);	
	pthread_sigmask(SIG_BLOCK, &set, NULL);
	while(compare_and_swap(&mutex->lock, 0, 1));
	if (list_empty(&mutex->waiter)) {
		mutex->count++;
	} else {
		struct thread *t = list_first_entry(&mutex->waiter, struct thread, list);
		list_del_init(&t->list);
		pthread_kill(t->pthread, SIGUSR1);
	}
	mutex->lock = 0;
	pthread_sigmask(SIG_UNBLOCK, &set, NULL);
	return;
}



/*********************************************************************
 * Ring buffer
 *********************************************************************/
struct ringbuffer {
	int in, out;
	/** NEVER CHANGE @nr_slots AND @slots ****/
	/**/ int nr_slots;                     /**/
	/**/ int *slots;                       /**/
	/*****************************************/
	
	
};

struct semaphore {
	int count;
	int lock;
	struct list_head waiter;
};

struct ringbuffer ringbuffer = {
	0, 0
};

struct semaphore empty, full;
struct mutex mutex;

void wait(struct semaphore *s) {
	sigset_t set;
	siginfo_t info;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR2);	
	pthread_sigmask(SIG_BLOCK, &set, NULL);
	while(compare_and_swap(&s->lock, 0, 1));
	s->count--;
	if(s->count < 0) {
		struct thread t;
		t.pthread = pthread_self();
		list_add_tail(&t.list, &s->waiter);
		s->lock = 0;
		sigwaitinfo(&set, &info);
	} else {
		s->lock = 0;
		pthread_sigmask(SIG_UNBLOCK, &set, NULL);
	}
	return;
}

void signal_s(struct semaphore *s) {
	sigset_t set;
	siginfo_t info;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR2);	
	pthread_sigmask(SIG_BLOCK, &set, NULL);
	while(compare_and_swap(&s->lock, 0, 1));
	s->count++;
	if(s->count <= 0) {
		if(!list_empty(&s->waiter)) {
			struct thread *t = list_first_entry(&s->waiter, struct thread, list);
			list_del_init(&t->list);
			pthread_kill(t->pthread, SIGUSR2);
		}
	}
	s->lock = 0; 
	pthread_sigmask(SIG_UNBLOCK, &set, NULL);
	return;
}

/*********************************************************************
 * enqueue_into_ringbuffer(@value)
 *
 * DESCRIPTION
 *   Generator in the framework tries to put @value into the buffer.
 */
void enqueue_into_ringbuffer(int value)
{	
	wait(&empty); // 버퍼에 남아 있는 공간(empty)이 0이면 대기
	acquire_mutex(&mutex);
	ringbuffer.slots[ringbuffer.in] = value;
	ringbuffer.in = (ringbuffer.in+1) % ringbuffer.nr_slots;
	release_mutex(&mutex);
	signal_s(&full);
}


/*********************************************************************
 * dequeue_from_ringbuffer(@value)
 *
 * DESCRIPTION
 *   Counter in the framework wants to get a value from the buffer.
 *
 * RETURN
 *   Return one value from the buffer.
 */
int dequeue_from_ringbuffer(void)
{
	wait(&full); // 채워져 있는 버퍼의 공간(full)이 0이면 대기
	acquire_mutex(&mutex);
	int value = ringbuffer.slots[ringbuffer.out];
	ringbuffer.out = (ringbuffer.out+1) % ringbuffer.nr_slots;
	release_mutex(&mutex);
	signal_s(&empty);
	return value;
}


/*********************************************************************
 * fini_ringbuffer
 *
 * DESCRIPTION
 *   Clean up your ring buffer.
 */
void fini_ringbuffer(void)
{
	free(ringbuffer.slots);
}

/*********************************************************************
 * init_ringbuffer(@nr_slots)
 *
 * DESCRIPTION
 *   Initialize the ring buffer which has @nr_slots slots.
 *
 * RETURN
 *   0 on success.
 *   Other values otherwise.
 */
int init_ringbuffer(const int nr_slots)
{
	/** DO NOT MODIFY THOSE TWO LINES **************************/
	/**/ ringbuffer.nr_slots = nr_slots;                     /**/
	/**/ ringbuffer.slots = malloc(sizeof(int) * nr_slots);  /**/
	/***********************************************************/
	empty.count = nr_slots;
	full.count = 0;
	empty.lock = 0;
	full.lock = 0;
	init_mutex(&mutex);
	INIT_LIST_HEAD(&empty.waiter);
	INIT_LIST_HEAD(&full.waiter);
	return 0;
}
