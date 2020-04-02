#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "queue.h"
#include "thread.h"
#include "tps.h"

struct TPS {
	struct Page *page;
	pthread_t tid;
};

struct Page {
	void *mem;
	int ref_count;
};

static queue_t pthreads;
static int initialized = 0;

static int find_tid(void *data, void *arg)
{
	struct TPS *a = (struct TPS*)data;
	pthread_t b = *(pthread_t *)arg;
	return a->tid == b;
}

/* Used for signal handler to find if the memory that triggers the sig fault is
 * in the TPS */
static int find_addr(void *data, void *arg)
{
	void *a = ((struct TPS*)data)->page->mem;
	return a == arg;
}

static void segv_handler(int sig, siginfo_t *si, __attribute__((unused)) void
		*context)
{
	/*
	 * Get the address corresponding to the beginning of the page where the
	 * fault occurred
	 */
	void *p_fault = (void*)((uintptr_t)si->si_addr & ~(TPS_SIZE - 1));
	/*
	 * Iterate through all the TPS areas and find if p_fault matches one of
	 * them
	 */
	void *result = NULL;
	queue_iterate(pthreads, find_addr, p_fault, &result);
	if (result != NULL)
		/* Printf the following error message */
		fprintf(stderr, "TPS protection error!\n");

	/* In any case, restore the default signal handlers */
	signal(SIGSEGV, SIG_DFL);
	signal(SIGBUS, SIG_DFL);
	/* And transmit the signal again in order to cause the program to crash
	 * */
	raise(sig);
}

int tps_init(int segv)
{
	/* Initialize signal handler for TPS' sigfault */
	if (initialized)
		return -1;
	if (segv) {
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = segv_handler;
		sigaction(SIGBUS, &sa, NULL);
		sigaction(SIGSEGV, &sa, NULL);
	}
	pthreads = queue_create();	
	initialized = 1;
	return 0;
}

int tps_create(void)
{
	enter_critical_section();
	pthread_t tid = pthread_self();
	void *result = NULL;
	queue_iterate(pthreads, find_tid, (void *)&tid, &result);
	/* TPS area already exist, quit the function right away as we don't
	 * need duplicate TPS */
	if (result != NULL) {
		exit_critical_section();
		return -1;
	}
	struct TPS *tps_area = malloc(sizeof(struct TPS));
	if (tps_area == NULL) {
		exit_critical_section();
		return -1;
	}
	tps_area->page = malloc(sizeof(struct Page));
	if (tps_area->page == NULL) {
		exit_critical_section();
		return -1;
	}
	/* Start with a reference count of 1 */
	tps_area->page->ref_count = 1;
	tps_area->tid = tid;
	/* memory page will have no r/w permission by default, with private and
	 * anonymous mapping */
	tps_area->page->mem = mmap((void *) TPS_SIZE , TPS_SIZE, PROT_NONE,
			MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	/* Check if mapping call is success */
	if (tps_area->page->mem == MAP_FAILED) {
		perror("mmap");
		exit_critical_section();
		return -1;
	}
	queue_enqueue(pthreads, (void *)tps_area);
	exit_critical_section();
	return 0;
}

/* For cleaning up the TPS */
int tps_destroy(void)
{
	enter_critical_section();
	pthread_t tid = pthread_self();
	struct TPS *tar = NULL;
	queue_iterate(pthreads, find_tid, (void *)&tid, (void **)&tar);
	if (tar == NULL) {
		exit_critical_section();
		return -1;
	}
	if (munmap(tar->page->mem, TPS_SIZE) == -1) {
		exit_critical_section();
		return -1;
	}
	/* Remove it from the queue aswell */
	queue_delete(pthreads, (void *)tar);
	free(tar->page);
	free(tar);
	exit_critical_section();
	return 0;
}

int tps_read(size_t offset, size_t length, void *buffer)
{
	enter_critical_section();
	if (offset + length > TPS_SIZE || buffer == NULL) {
		exit_critical_section();
		return -1;
	}
	pthread_t tid = pthread_self();
	struct TPS *src = NULL;
	queue_iterate(pthreads, find_tid, (void *)&tid, (void **)&src);
	if (src == NULL) {
		exit_critical_section();
		return -1;
	}
	/* Temporarily change the memory page to be readable, so we can copy
	 * over the to the target buffer */
	if (mprotect(src->page->mem, TPS_SIZE, PROT_READ)) {
		exit_critical_section();
		return -1;
	}
	void *result = memcpy(buffer, src->page->mem + offset, length - offset);
	if (result == NULL) {
		perror("memcpy");
		exit_critical_section();
		return -1;
	}
	/* Change it back to no r/w access at the end */
	if (mprotect(src->page->mem, TPS_SIZE, PROT_NONE)) {
		exit_critical_section();
		return -1;
	}
	exit_critical_section();
	return 0;
}

int tps_write(size_t offset, size_t length, void *buffer)
{
	enter_critical_section();
	if (offset + length > TPS_SIZE || buffer == NULL) {
		exit_critical_section();
		return -1;
	}
	pthread_t tid = pthread_self(); 
	struct TPS *dest = NULL;
	queue_iterate(pthreads, find_tid, (void *)&tid, (void **)&dest);
	if (dest == NULL) {
		exit_critical_section();
		return -1;
	}
	/* Check if the page is referenced by more than one thread, we will
	 * create it's own page memory */
	if (dest->page->ref_count > 1) {
		/* Store the original page */
		struct Page *tmp = dest->page;
		dest->page = malloc(sizeof(struct Page));
		/* Make a new page for the TPS */
		dest->page->mem = mmap((void *) TPS_SIZE , TPS_SIZE,
				PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
		if (mprotect(tmp->mem, TPS_SIZE, PROT_READ)) {
			exit_critical_section();
			return -1;
		}
		if (mprotect(dest->page->mem, TPS_SIZE, PROT_WRITE)) {
			exit_critical_section();
			return -1;
		}
		memcpy(dest->page->mem, tmp->mem, TPS_SIZE);
		dest->page->ref_count = 1;
		tmp->ref_count--;
		if (mprotect(tmp->mem, TPS_SIZE, PROT_NONE)) {
			exit_critical_section();
			return -1;
		}
		if (mprotect(dest->page->mem, TPS_SIZE, PROT_NONE)) {
			exit_critical_section();
			return -1;
		}
	}
	if (mprotect(dest->page->mem, TPS_SIZE, PROT_WRITE)) {
		exit_critical_section();
		return -1;
	}
	void *result = memcpy(dest->page->mem, buffer + offset, length);
	if (result == NULL) {
		perror("memcpy");
		exit_critical_section();
		return -1;
	}
	if (mprotect(dest->page->mem, TPS_SIZE, PROT_NONE)) {
		exit_critical_section();
		return -1;
	}
	exit_critical_section();
	return 0;
}

int tps_clone(pthread_t tid)
{
	enter_critical_section();
	struct TPS *dest = NULL;
	struct TPS *src = NULL;
	pthread_t self_tid = pthread_self();
	queue_iterate(pthreads, find_tid, (void *)&self_tid, (void **)&dest);
	queue_iterate(pthreads, find_tid, (void *)&tid, (void **)&src);
	/* Exit right away if the thread requesting the clone already has a
	 * TPS, or if we could not find the target tid */
	if (dest != NULL || src == NULL) {
		exit_critical_section();
		return -1;
	}
	dest = malloc(sizeof(struct TPS));
	queue_iterate(pthreads, find_tid, (void *)&self_tid, (void **)&dest);
	/* Points to the same page */
	dest->page = src->page;
	dest->tid = self_tid;
	/* Increments the reference count of the page */
	src->page->ref_count++;
	queue_enqueue(pthreads, (void *)dest);
	exit_critical_section();
	return 0;
}

