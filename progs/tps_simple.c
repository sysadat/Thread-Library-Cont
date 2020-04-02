#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <tps.h>
#include <sem.h>

static char msg1[TPS_SIZE] = "Hello world!\n";
static char msg2[TPS_SIZE] = "hello world!\n";

static sem_t sem1, sem2;

void *thread2(__attribute__((unused)) void *arg)
{
	char *buffer = malloc(TPS_SIZE);

	/* Create TPS and initialize with *msg1 */
	tps_create();
	tps_write(0, TPS_SIZE, msg1);

	/* Read from TPS and make sure it contains the message */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg1, buffer, TPS_SIZE));
	printf("thread2: read OK!\n");

	/* Transfer CPU to thread 1 and get blocked */
	sem_up(sem1);
	sem_down(sem2);

	/* When we're back, read TPS and make sure it sill contains the original */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg1, buffer, TPS_SIZE));
	printf("thread2: read OK!\n");

	/* Test writing with an offset */
	tps_write(4, TPS_SIZE - 4, msg1);
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg1 + 4, buffer, TPS_SIZE - 4));
	memset(buffer, 0, TPS_SIZE);
	
	/* Test reading with an offset */
	tps_write(0, TPS_SIZE, msg1);
	tps_read(3, TPS_SIZE - 3, buffer);
	assert(!memcmp(msg1 + 3, buffer, TPS_SIZE - 3));
	memset(buffer, 0, TPS_SIZE);
	printf("Reading and writing with offsets OK!\n");

	/* Error testing with invalid parameters */
	int retval;

	retval = tps_write(0, TPS_SIZE, NULL);
	assert(retval == -1);
	retval = tps_read(0, TPS_SIZE, NULL);
	assert(retval == -1);
	retval = tps_read(0, TPS_SIZE + 10, buffer);
	assert(retval == -1);
	retval = tps_write(0, TPS_SIZE + 10, msg1);
	assert(retval == -1);
	retval = tps_read(TPS_SIZE + 10, TPS_SIZE, buffer);
	assert(retval == -1);
	retval = tps_write(TPS_SIZE + 10, TPS_SIZE, msg1);
	assert(retval == -1);

	/* Transfer CPU to thread 1 and get blocked */
	sem_up(sem1);
	sem_down(sem2);

	/* Destroy TPS and quit */
	tps_destroy();
	free(buffer);
	return NULL;
}

void *thread1(__attribute__((unused)) void *arg)
{
	pthread_t tid;
	char *buffer = malloc(TPS_SIZE);

	/* Create thread 2 and get blocked */
	pthread_create(&tid, NULL, thread2, NULL);
	sem_down(sem1);

	/* When we're back, clone thread 2's TPS */
	tps_clone(tid);

	/* Read the TPS and make sure it contains the original */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg1, buffer, TPS_SIZE));
	printf("thread1: read OK!\n");

	/* Modify TPS to cause a copy on write */
	buffer[0] = 'h';
	tps_write(0, 1, buffer);

	/* Transfer CPU to thread 2 and get blocked */
	sem_up(sem2);
	sem_down(sem1);

	/* When we're back, make sure our modification is still there */
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!strcmp(msg2, buffer));
	printf("thread1: read OK!\n");

	/* Transfer CPU to thread 2 */
	sem_up(sem2);

	/* Wait for thread2 to die, and quit */
	pthread_join(tid, NULL);
	tps_destroy();
	free(buffer);
	return NULL;
}

int main(void)
{
	pthread_t tid;

	/* Create two semaphores for thread synchro */
	sem1 = sem_create(0);
	sem2 = sem_create(0);

	/* Init TPS API */
	tps_init(1);

	/* Create thread 1 and wait */
	pthread_create(&tid, NULL, thread1, NULL);
	pthread_join(tid, NULL);

	/* Destroy resources and quit */
	sem_destroy(sem1);
	sem_destroy(sem2);
	return 0;
}
