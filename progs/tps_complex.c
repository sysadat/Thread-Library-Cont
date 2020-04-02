#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

static sem_t sem1, sem2;
static char msg[TPS_SIZE] = "Hellloo!!\n";

void *thread2(__attribute__((unused)) void *arg)
{
	char *buffer = malloc(TPS_SIZE);
	tps_create();
	void *random = malloc(TPS_SIZE);
	void *void_buf = malloc(TPS_SIZE);
	tps_write(0, TPS_SIZE, random);
	tps_read(0, TPS_SIZE, void_buf);
	assert(!memcmp(void_buf, random, TPS_SIZE));
	/* Test void* instead of just char* */
	printf("Void pointer read and write OK!\n");
	tps_write(0, TPS_SIZE, msg);
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg, buffer, TPS_SIZE));
	printf("Normal char pointer read and write OK!\n");
	sem_up(sem1);
	sem_down(sem2);
	tps_write(2, TPS_SIZE - 2, msg);
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	assert(!memcmp(msg + 2, buffer, TPS_SIZE - 2));
	sem_up(sem1);
	tps_destroy();
	free(buffer);
	free(random);
	return NULL;
}

void *thread1(__attribute__((unused)) void *arg)
{
	pthread_t tid;
	pthread_create(&tid, NULL, thread2, NULL);
	sem_down(sem1);
	tps_clone(tid);
	char *buffer = malloc(TPS_SIZE);
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	/* If cloned successfully, it should contain msg */
	assert(!memcmp(buffer, msg, TPS_SIZE));
	printf("Clone and read successful\n");
	sem_up(sem2);
	pthread_join(tid, NULL);
	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);
	/* The clone should still have msg */ 
	assert(!memcmp(msg, buffer, TPS_SIZE));
	memset(buffer, 0, TPS_SIZE);
	int retval = tps_read(4095, 2, buffer);
	assert(retval == -1);
	tps_destroy();
	free(buffer);
	return NULL;
}

int main(void)
{
	pthread_t tid;
	sem1 = sem_create(0);
	sem2 = sem_create(0);
	tps_init(1);
	pthread_create(&tid, NULL, thread1 , NULL);
	pthread_join(tid, NULL);
	sem_destroy(sem1);
	sem_destroy(sem2);
	return 0;
}
