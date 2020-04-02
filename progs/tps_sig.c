#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

void *latest_mmap_addr;
void *__real_mmap(void *addr, size_t len, int prot, int flags, int fildes,
		off_t off);
void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fildes,
		off_t off)
{
	latest_mmap_addr = __real_mmap(addr, len, prot, flags, fildes, off);
	return latest_mmap_addr;
}

static sem_t sem1, sem2;
#define TEST_ASSERT(assert)			\
do {						\
	printf("ASSERT: " #assert " ... ");	\
	if (assert) {				\
		printf("PASS\n");		\
	} else	{				\
		printf("FAIL\n");		\
		exit(1);			\
	}					\
} while(0)

#define PRINT_SUCCESS(test) fprintf(stderr, "[" "\x1b[32m" "PASS" "\x1b[0m" "]" " \
%s\n", test)

void *thread2(__attribute__((unused)) void *arg)
{
	tps_create();
	sem_up(sem1);
	sem_down(sem2);
	tps_destroy();
	return NULL;
}

void *thread3(__attribute__((unused)) void *arg)
{
	pthread_t tid;
	int retval;
	/* Test invalid parameters for cloning */
	retval = tps_clone(100);
	TEST_ASSERT(retval == -1);
	retval = tps_clone(-1);
	TEST_ASSERT(retval == -1);

	PRINT_SUCCESS("clone fail");
	pthread_create(&tid, NULL, thread2, NULL);
	sem_down(sem1);
	retval = tps_clone(tid);
	TEST_ASSERT(retval == 0);
	PRINT_SUCCESS("clone success");

	static char msg[TPS_SIZE] = "Hellooooo\n";
	char *buffer = malloc(TPS_SIZE);
	/* Normal tps read and write to the buffer */
	tps_write(0, TPS_SIZE, msg);
	tps_read(0, TPS_SIZE, buffer);
	memset(buffer, 0, TPS_SIZE);
	/* This should trigger the TPS protection error, as we are trying to
	 * modify memory from protected memory */
	char *protected = latest_mmap_addr;
	protected[0] = '\0';
	return NULL;	
}

int main(void)
{
	pthread_t tid;
	sem1 = sem_create(0);
	sem2 = sem_create(0);
	pthread_create(&tid, NULL, thread3, NULL);
	int retval = tps_clone(tid);
	TEST_ASSERT(retval == -1);
	PRINT_SUCCESS("clone uninitialized");
	tps_init(1);
	retval = tps_init(1);
	TEST_ASSERT(retval == -1);
	PRINT_SUCCESS("initialized");
	pthread_join(tid, NULL);
	return 0;
}
