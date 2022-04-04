#include <stdio.h>
#include <unistd.h>
#include <printf.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>

#include "gfserver.h"
#include "cache-student.h"
#include "shm_channel.h"
#include "simplecache.h"

#if !defined(CACHE_FAILURE)
    #define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE

#define MAX_CACHE_REQUEST_LEN 82021 
sem_t* semCache;
//steque_t* queue;

pthread_mutex_t mux = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mux2 = PTHREAD_MUTEX_INITIALIZER;

void* bossWorker(void* arg) {
	//char buffer[256];
	while (1) {
		char* buffer = (char*)malloc(256);
		mqueue_read(buffer);
		pthread_mutex_lock(&mux);
		steque_enqueue(queue, buffer);
		//printf("Buffer: %s\n", buffer);
		pthread_mutex_unlock(&mux);
		sem_post(semCache);
		//buffer = NULL; 
	}
}

void* cacheWorkerFunction(void* arg) {
	while (1) {
		sem_wait(semCache);
		char message[256];
		pthread_mutex_lock(&mux2);
		char* msg = (char*)steque_pop(queue);
		pthread_mutex_unlock(&mux2);
		memcpy(message, msg, 255);
		free(msg);
		msg = NULL;
		char path[256];
		int i;
		int file_len;
		int bytes_transferred;
		char buffer[3500];
		int fildes = -1;
		struct stat statbuf;
		sscanf(message, "%s %d", path, &i);
		shm_object* shm = shm_get_object(i);
		fildes = simplecache_get(path);
		if (0 > fstat(fildes, &statbuf)) {
			file_len = 0;
			shm_write_fileLength(shm, file_len);
			shm_post_semaphore(shm);
			munmap(shm, segmentSize);
			continue;
		}
		else {
			file_len = (size_t)statbuf.st_size;
			shm_write_fileLength(shm, file_len);
			shm_post_semaphore(shm);
		}
		bytes_transferred = 0;
		int k = 0;
		bzero(buffer, 3500);
		while (bytes_transferred < file_len) {
			int remaining = file_len - bytes_transferred;
			if (3499 > remaining) {
				k = pread(fildes, buffer, remaining, bytes_transferred);
			}
			else {
				k = pread(fildes, buffer, 3499, bytes_transferred);
			}
			shm_write_data(shm, buffer);
			shm_post_semaphore_r(shm);
			bytes_transferred += k;
			bzero(buffer, 3500);
			shm_wait_semaphore_w(shm);
		}
		munmap(shm, segmentSize);
	}
}

static void _sig_handler(int signo){
	if (signo == SIGTERM || signo == SIGINT){
		sem_unlink("/CACHE");
		steque_destroy(queue);
		free(queue);
		//pthread_mutex_destroy(&mux);
		//pthread_mutex_destroy(&mux2);
		//mqueue_close();
		//shm_cleanup();
		//mqueue_clear();
		exit(signo);
	}
}

unsigned long int cache_delay;

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Thread count for work queue (Default is 42, Range is 1-235711)\n"      \
"  -d [delay]          Delay in simplecache_get (Default is 0, Range is 0-2500000 (microseconds)\n "	\
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"help",               no_argument,            NULL,           'h'},
  {"nthreads",           required_argument,      NULL,           't'},
  {"hidden",			 no_argument,			 NULL,			 'i'}, /* server side */
  {"delay", 			 required_argument,		 NULL, 			 'd'}, // delay.
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}




int main(int argc, char **argv) {
	int nthreads = 7;
	char *cachedir = "locals.txt";
	char option_char;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "id:c:hlxt:", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			default:
				Usage();
				exit(1);
			case 'h': // help
				Usage();
				exit(0);
				break;    
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;
            case 'c': //cache directory
				cachedir = optarg;
				break;
            case 'd':
				cache_delay = (unsigned long int) atoi(optarg);
				break;
			case 'i': // server side usage
			case 'u': // experimental
			case 'j': // experimental
				break;
		}
	}

	if (cache_delay > 2500000) {
		fprintf(stderr, "Cache delay must be less than 2500000 (us)\n");
		exit(__LINE__);
	}

	if ((nthreads>235711) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads must be in between 1-235711\n");
		exit(__LINE__);
	}

	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}

	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}

	simplecache_init(cachedir);
	mqueue_init();
	char sizeData[256];
	mqueue_read(sizeData);
	size_t segsize;
	unsigned int nsegments;
	sscanf(sizeData, "%zu %u", &segsize, &nsegments);
	shm_init(nsegments, segsize);
	queue = (steque_t*)malloc(sizeof(steque_t));
	steque_init(queue);
	semCache = sem_open("/CACHE", O_CREAT, S_IRUSR | S_IWUSR, 0);
	pthread_t bossThread;
	pthread_t thread[nthreads];
	for(int i = 0; i < nthreads; i++){
		pthread_create(&thread[i], NULL, cacheWorkerFunction, NULL);
	}
	pthread_create(&bossThread, NULL, bossWorker, NULL);

	void* ret;
	for(int i = 0; i < nthreads; i++){
		pthread_join(thread[i], &ret);
	}

	// Won't execute
	return -1;
}
