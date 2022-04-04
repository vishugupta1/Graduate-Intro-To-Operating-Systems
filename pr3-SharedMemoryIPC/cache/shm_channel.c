#include "shm_channel.h"

// code for shared memory ----------------------------------------

void shm_init(unsigned int nsegments, size_t segmentsize) {
	numberSegments = nsegments;
	segmentSize = segmentsize;
}

shm_object* shm_create(int i) {
	shm_object* shm; 
	char key[256];
	sprintf(key, "%s %d", prefix, i);
	int fd = shm_open(key, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if (fd > 0)
		ftruncate(fd, segmentSize);
	shm = mmap(NULL, segmentSize, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	pthread_mutex_init(&(shm->muxSegment), NULL);
	shm->fileLength = -1; 
	sem_init(&(shm->semSegment), 1, 0);
	sem_init(&(shm->semReader), 1, 0);
	sem_init(&(shm->semWriter), 1, 0);
	return shm;
}

shm_object* shm_get_object(int i) {
	shm_object* shm;
	char key[256];
	sprintf(key, "%s %d", prefix, i);
	int fd = shm_open(key, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	shm = mmap(NULL, segmentSize, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	return shm;
}

void shm_cleanup() {
	for (int i = 0; i < numberSegments; i++) {
		char key[256];
		sprintf(key, "%s %d", prefix, i);
		shm_unlink(key);
	}
}

void shm_cleanup_single(int i) {
	char key[256];
	sprintf(key, "%s %d", prefix, i);
	shm_unlink(key);
}

void shm_write_fileLength(shm_object* shm, int length) {
	shm->fileLength = length;
}

int shm_read_fileLength(shm_object* shm) {
	return shm->fileLength;
}

int shm_read_data(shm_object* shm, char* data) {
	memcpy(data, shm->buffer, 3499);
	return 3499;
}

ssize_t shm_write_data(shm_object* shm, const char* data) {
	bzero(shm->buffer, 3500);
	memcpy(shm->buffer, data, 3499);
	return 1;
}

void shm_wait_semaphore(shm_object* shm) {
    sem_wait(&(shm->semSegment));
}

void shm_post_semaphore(shm_object* shm) {
	sem_post(&(shm->semSegment));
}

void shm_wait_semaphore_r(shm_object* shm) {
	sem_wait(&(shm->semReader));
}

void shm_post_semaphore_r(shm_object* shm) {
	sem_post(&(shm->semReader));
}

void shm_wait_semaphore_w(shm_object* shm) {
	sem_wait(&(shm->semWriter));
}

void shm_post_semaphore_w(shm_object* shm) {
	sem_post(&(shm->semWriter));
}

// code for mqueue -----------------------------------------------

void mqueue_init() {
	struct mq_attr attr;
	attr.mq_flags = 0;
	attr.mq_maxmsg = 10;
	attr.mq_msgsize = 256;
	attr.mq_curmsgs = 0;

	mq = mq_open(mqueue, O_CREAT | O_RDWR, 0666, &attr);
	if (mq < 0) {
		mq = mq_open(mqueue, 0, 0666, &attr);
	}
}

ssize_t mqueue_read(char* message) {
	ssize_t n = mq_receive(mq, message, 256, 0);
	if (n < 0) {
		perror("Failed recieving message from message queue\n");
	}
	return n;
}

int mqueue_write(const char* data) {
	int n = mq_send(mq, data, strlen(data) + 1, 0);
	if (n < 0) {
		perror("Failure writing message to message queue\n");
	}
	return n;
}

void mqueue_close() {
	if (mq_close(mq) < 0) {
		perror("Failure closing mqueue descriptor\n");
	}
}

void mqueue_clear() {
	mqueue_close();
	mq_unlink(mqueue);
}
