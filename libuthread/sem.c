#include <stddef.h>
#include <stdlib.h>

#include "queue.h"
#include "sem.h"
#include "thread.h"

struct semaphore {
	unsigned int size;
	queue_t blocked_threads;
};

sem_t sem_create(size_t count)
{
	sem_t sem = malloc(sizeof(sem_t));
	if (!sem) {
		return NULL;
	}

	sem -> size = count;
	sem -> blocked_threads = queue_create();
	if (!sem -> blocked_threads) {
		return NULL;
	}
	return sem;
}

int sem_destroy(sem_t sem)
{
	// If sem is NULL or there are still blocked_threads, return -1
	if (!sem || queue_length(sem -> blocked_threads) > 0) {
		return -1;
	}
	free(sem);
	return 0;
}

int sem_down(sem_t sem)
{
	if (!sem) {
		return -1;
	}
	/* Want to ensure mutual exclusion with other threads */
	enter_critical_section();

	/* Taking an unavailable semaphore will cause the caller thread to be
	 * blocked until the semaphore becomes available */
	if (!sem -> size) {
		pthread_t tid = pthread_self();
		queue_enqueue(sem -> blocked_threads, (void*)tid);

		/* Exit the critical section before going to sleep and re-enter
		 * the critical section upon wake-up.*/
		thread_block();
	}
	sem -> size -= 1;

	/* Want to allow another thread who was waiting to enter the same
	 * critical  section to enter */
	exit_critical_section();

	return 0;
}

int sem_up(sem_t sem)
{
	if (!sem) {
		return -1;
	}

	/* Want to ensure mutual exclusion with other threads */
	enter_critical_section();
	if (queue_length(sem -> blocked_threads) > 0) {
		pthread_t tid;
		queue_dequeue(sem -> blocked_threads, (void**)&tid);

		/* Unblock thread @tid and make it ready for scheduling */
		thread_unblock(tid);
	}
	sem -> size += 1;

	/* Want to allow another thread who was waiting to enter the same
	 * critical  section to enter */
	exit_critical_section();

	return 0;
}

int sem_getvalue(sem_t sem, int *sval)
{
	if (!sem || !sval) {
		return -1;
	}

	int thread_count = queue_length(sem -> blocked_threads);
	if (thread_count  == -1) {
		return -1;
	}

	if (sem -> size > 0) {
		*sval = sem -> size;
	} else if (!sem -> size) {
		*sval = thread_count * -1;
	}

	return 0;
}
