/* In case you want to implement the shared memory IPC as a library... */


#ifndef _SHM_CHANNEL_H_
#define _SHM_CHANNEL_H_

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdlib.h>
#include "steque.h"

#define prefix "/shared memory"
#define mqueue "/mqueue"
#define semaphore "/semaphore"

unsigned int numberSegments;
size_t segmentSize;
mqd_t mq;
steque_t* queue;

// code for shared memory -----------------------------

typedef struct shm_object {
	pthread_mutex_t muxSegment; 
	sem_t semSegment;
	sem_t semWriter; 
	sem_t semReader; 
	int fileLength; 
	char buffer[4000];
}shm_object;

void shm_init(unsigned int nsegments, size_t segmentsize);

void shm_cleanup();

void shm_cleanup_single(int i);

shm_object* shm_create(int i);

shm_object* shm_get_object(int i);

void shm_write_fileLength(shm_object* shm, int length);

int shm_read_fileLength(shm_object* shm);

int shm_read_data(shm_object* shm, char* data);

ssize_t shm_write_data(shm_object* shm, const char* data);

void shm_wait_semaphore(shm_object* shm);

void shm_post_semaphore(shm_object* shm);

void shm_wait_semaphore_r(shm_object* shm);

void shm_post_semaphore_r(shm_object* shm);

void shm_wait_semaphore_w(shm_object* shm);

void shm_post_semaphore_w(shm_object* shm);

// code for mqueue ------------------------------------

void mqueue_init();

ssize_t mqueue_read(char* message);

int mqueue_write(const char* data);

void mqueue_close();

void mqueue_clear();

#endif